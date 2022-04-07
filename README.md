# odbc
ODBC로 DB 처리를 돕기 위해 제작

# 특징
- ODBC API 래핑을 통해 간단한 사용법
- OdbcManager를 통해 Odbc(연결) 객체 풀링
- 연결 풀을 관리하며 객체 할당을 stack으로 관리
- 각 연결에 대한 health 체크는 따로 하지 않으며 사용 중인 객체의 여결 문제가 생길 경우 뒤 연결들은 모두 파기

# 빌드
- C++17 또는 최신 컴파일러 필요
- cmake 3.0 이상 버전 필요
- cmake 빌드(Visual Studio 2019 기준)
```
> cmake -G 를 사용하여 Generators 확인
> cmake ./ -G "Visual Studio 16 2019"
> 솔루션 파일(.sln) 오픈하여 빌드
```

# 요구 사항
- 각 DBMS에 맞는 ODBC Driver 설치 필요
- MS-SQL에서 MARS 사용 시 msodbcsql.h 추가 필요

# 예정 작업
- MySQL, MariaDB 테스트 환경 구축
- endpoint에 대한 health check 기능 추가 필요하며 이건 각 연결 객체에 대한 체크는 아님.
- Odbc 객체 사용 중 Critical 이슈 발생 시 정책 필요.(권장 : 롤1. ODBC에서는 에러에 대한 상태만을 관리하며 실제 처리는 사용자가 해야함.) 
롤1) 현재 요청을 실패 처리하고 해당 유저의 연결을 종료한다.
- 장점 : 유저가 보는 정보와 서버의 정보가 꼬일 일이 별로 없다.
- 단점 : 순간적인 DB 이슈가 발생하더라도 DB를 이용하게되는 유저들 모두 연결이 종료 될 수 있다.(범위는 항상 다름)

롤2) 연결이 복구 될때까지 대기 한다.
- 장점 : 순간적인 DB 이슈가 발생하더라도 유저에게응 영향을 주지 않는다.
- 단점 :
1) DB 이슈가 지속될 경우 메모리 정보와 DB 정보 동기화가 깨진다.
2) Q에 DB 요청이 쌓여 메모리가 가득차는 상황이 만들어질 수 있으며 이때 크래시 발생할 경우 모든 DB 처리가 날라간다. 이렇게되면 클라에서의 유저가 획득한 아이템 및 여러 값들이 롤백되는 현상을 경험하게된다.

# 사용 예
1) IDataAccessObject 구현
```cpp
class P_GAME_DAILY_ACHIEVEMENT_R : public IDataAccessObject
{
public:
	// 데이터 구조체 선언
	struct element
	{
		int32_t type;
		std::string info;
		std::string expiretime;
	};

	// Query 실행 중 에러 발생 시 호출
	virtual void HandleOdbcException(_odbc_error_ptr_t& err) override
	{
	}

	// RecordSet 데이터 파싱
	// 1개 이상의 row를 받았을 경우 처리
	virtual bool Parse(Statement* statement) override
	{
		std::string expireTime;

		std::vector<element> results;

		do
		{
			element elem;
			statement->ReadData(elem.type);
			statement->ReadData(elem.info);
			statement->ReadData(elem.expiretime);

			results.emplace_back(elem);
		} while (true == statement->MoveNext());

		return true;
	}

	// 모든 레코드셋 Parse가 끝난 후 호출되는 콜백
	// 데이터 로드가 끝난 상태이므로 필요한 후 처리를 진행
	virtual void Process() override 
	{
		std::cout << __FUNCTION__ << std::endl;
	}
};
```
2. Query객체 Factory 등록
```cpp
class NamedQuery
{
public:
	// Query 선언 <- Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>
	static Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>* CreateP_GAME_DAILY_ACHIEVEMENT_R()
	{
		// 객체 생성 시 Query 작성
		return new Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>(
			"{ call P_GAME_DAILY_ACHIEVEMENT_R(?, ?) }"
			);
	}
};

```
3. 로깅 객체 생성
```cpp
class Logging : public ILogging
{
public:
	virtual void Trace(std::string_view message) override
	{
		std::cout << message << std::endl;
	}

	virtual void Debug(std::string_view message) override
	{
		std::cout << message << std::endl;
	}

	virtual void Info(std::string_view message) override
	{
		std::cout << message << std::endl;
	}

	virtual void Warning(std::string_view message) override
	{
		std::cout << message << std::endl;
	}

	virtual void Error(std::string_view message) override
	{
		std::cout << message << std::endl;
	}
};
```

4. 쿼리 실행
```cpp
int main() 
{	
	// 1. ODBC 매니저 초기화 및 Connection 획득
	OdbcManager odbcManager;
	// 사용자 정의 로깅 객체 생성
	_logging_ptr_t logging = std::make_shared<Logging>();
	odbcManager.AttachLogging(logging);
	odbcManager.Init("Driver={ODBC Driver 17 for SQL Server};Server=tcp:172.31.101.38,1433;Database=MFR_GAME;Uid=MFRServerUser;Pwd=1234;language=english;ConnectRetryCount=0;", 10, 100);

	auto connection = odbcManager.GetConnection();
  
	// 2. P_GAME_DAILY_ACHIEVEMENT_R 실행
	{
		int64_t usn = 1000121111200000002;
		std::string datetime = "2022-03-23 12:12:12";

		auto query = NamedQuery::CreateP_GAME_DAILY_ACHIEVEMENT_R();
		query->SetParameter(usn, datetime);

		// 질의 정보를 등록
		connection->BindQuery(query);
		connection->Execute();
	}
  
 	// 반환
	odbcManager.Release(connection);

	// 종료 처리
	odbcManager.CleanUp();

	return 0;
}
```
