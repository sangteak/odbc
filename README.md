# odbc
ODBC로 DB 처리를 돕기 위해 제작

# 특징
- ODBC API 래핑을 통해 간단한 사용법
- OdbcManager를 통해 Odbc(연결) 객체 풀링

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
