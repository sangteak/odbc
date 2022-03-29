#include <windows.h>
#include "odbc.h"
#include <iostream>
#include <cstdint>
#include <vector>

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

// MARS 읽기 처리
class P_GAME_LoginData_MARS_RU : public IDataAccessObject
{
public:
	struct SP_RESULT
	{
		int32_t spRtn;
		int32_t isNewUser;
	};

	struct TB_G_USER_INFO
	{
		int64_t usn;
		std::string pid;
		std::string country;

		// ...
	};

	struct TB_G_USER_SLOT
	{
		int32_t slotNo;
		int64_t csn;
		std::string lastPlayTime;
	};
	using _user_slot_container_t = std::vector<TB_G_USER_SLOT>;

	struct TB_G_CHARACTER
	{
		int64_t csn;
		int64_t usn;
		int32_t characterId;
	};
	using _character_container_t = std::vector<TB_G_CHARACTER>;

	struct TB_G_CHARACTER_PRESET
	{
		int64_t csn;
		int32_t presetType;
		int32_t slotNo;
		int32_t itemID;
		int64_t isn;
	};
	using _character_preset_container_t = std::vector<TB_G_CHARACTER_PRESET>;

	void Read_SP_RESULT(Statement* statement)
	{
		statement->ReadData(m_spResult.spRtn);
		statement->ReadData(m_spResult.isNewUser);
	}

	void Read_TB_G_USER_INFO(Statement* statement)
	{
		statement->ReadData(m_userInfo.usn);
		statement->ReadData(m_userInfo.pid);
	}

	void Read_TB_G_USER_SLOT(Statement* statement)
	{
		do 
		{
			TB_G_USER_SLOT userSlot;
			statement->ReadData(userSlot.slotNo);
			statement->ReadData(userSlot.csn);
			statement->ReadData(userSlot.lastPlayTime);
			
			m_userSlotList.emplace_back(std::move(userSlot));
		} while (statement->MoveNext());
	}

	void Read_TB_G_CHARACTER(Statement* statement)
	{
		do {
			TB_G_CHARACTER character;
			statement->ReadData(character.csn);
			statement->ReadData(character.usn);
			statement->ReadData(character.characterId);

			m_characters.emplace_back(std::move(character));
		} while (statement->MoveNext());
	}

	void Read_TB_G_CHARACTER_PRESET(Statement* statement)
	{		
		do {
			TB_G_CHARACTER_PRESET characterPreset;
			statement->ReadData(characterPreset.csn);
			statement->ReadData(characterPreset.presetType);
			statement->ReadData(characterPreset.slotNo);
			statement->ReadData(characterPreset.itemID);
			statement->ReadData(characterPreset.isn);

			m_characterPresetList.emplace_back(std::move(characterPreset));
		} while (statement->MoveNext());
	}

	virtual void HandleOdbcException(_odbc_error_ptr_t& err) override
	{
		// Query 처리 중 ODBC 관련 에러가 발생 했을 경우 필요한 처리를 진행합니다.
		// ex) 패킷 전송을 통해 상황을 알림
	}

	virtual bool Parse(Statement* statement) override
	{
		static constexpr int32_t INDEX_SP_RESULT = 0;
		static constexpr int32_t INDEX_TB_G_USER_INFO = 1;
		static constexpr int32_t INDEX_TB_G_USER_SLOT = 2;
		static constexpr int32_t INDEX_TB_G_CHARACTER = 3;
		static constexpr int32_t INDEX_TB_G_CHARACTER_PRESET = 4;
		
		//if (Statement::eFetchResult::EMPTY == statement->GetFetchResult())
		if (true == statement->IsNoData())
		{
			return true;
		}

		switch (statement->GetRecordsetIndex())
		{
		case INDEX_SP_RESULT:
			Read_SP_RESULT(statement);
			break;
		case INDEX_TB_G_USER_INFO:
			Read_TB_G_USER_INFO(statement);
			break;

		case INDEX_TB_G_USER_SLOT:
			Read_TB_G_USER_SLOT(statement);
			break;

		case INDEX_TB_G_CHARACTER:
			Read_TB_G_CHARACTER(statement);
			break;

		case INDEX_TB_G_CHARACTER_PRESET:
			Read_TB_G_CHARACTER_PRESET(statement);
			break;
		}

		return true;
	}

	// 분석(Parse 호출)이 끝났을음 알리는 이벤트 
	virtual void Process() override
	{
		std::cout << __FUNCTION__ << std::endl;
	}

private:
	SP_RESULT m_spResult;
	TB_G_USER_INFO m_userInfo;
	_user_slot_container_t m_userSlotList;
	_character_container_t m_characters;
	_character_preset_container_t m_characterPresetList;
};

// Query Factory
class NamedQuery
{
public:
	// Query 선언 <- Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>
	using _query_P_GAME_DAILY_ACHIEVEMENT_R = std::shared_ptr<Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>>;
	static _query_P_GAME_DAILY_ACHIEVEMENT_R CreateP_GAME_DAILY_ACHIEVEMENT_R()
	{
		// 객체 생성 시 Query 작성
		return std::make_shared<Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>>(
			"{ call P_GAME_DAILY_ACHIEVEMENT_R(?, ?) }"
			);
	}

	using _query_P_GAME_LoginData_MARS_RU_t = Query<P_GAME_LoginData_MARS_RU,
		uint8_t,
		int64_t,
		std::string,
		int32_t,
		std::string,
		std::string,
		std::string,
		std::string>;

	static _query_P_GAME_LoginData_MARS_RU_t* CreateP_GAME_LoginData_MARS_RU()
	{
		return new _query_P_GAME_LoginData_MARS_RU_t(
			"{ call P_GAME_LoginData_MARS_RU(?,?,?,?,?,?,?,?) }"
		);
	}
};

#include <queue>

class TlsWorkerThread
{
public:
	TlsWorkerThread(OdbcConfiguration& config) 
	{
		m_odbcPoolTls.SetConfiguration(config);
	}

	void Run() 
	{
		for (int32_t i = 0; i < 1; ++i)
		{
			m_threadGroup.emplace_back(
				[this]()
				{
					auto odbcPool = m_odbcPoolTls.Create();
					if (false == odbcPool->HasLogging())
					{
						odbcPool->AttachLogging(m_logging);
					}

					/*
					/////////////////////////////////////////////////////////////
					// Queue를 통해 전달된 Task
					int64_t usn = 1000121111200000002;
					std::string datetime = "2022-03-23 12:12:12";

					auto query = NamedQuery::CreateP_GAME_DAILY_ACHIEVEMENT_R();
					query->SetParameter(usn, datetime);
					/////////////////////////////////////////////////////////////
					*/

					while (true)
					{
						std::shared_ptr<IQuery> query;
						if (false == TryPop(query))
						{
							std::this_thread::sleep_for(std::chrono::milliseconds(10));
							continue;
						}

						if (nullptr == query)
						{
							break;
						}

						do
						{
							auto connection = odbcPool->GetConnection();
							if (nullptr == connection)
							{
								// 네트웍 또는 장비 이슈로 인해 연결을 할수 없는 상태라면?? 

								break;
							}

							// 질의 정보를 등록
							connection->BindQuery(query.get());
							auto executeResultCode = connection->Execute();
							if (SQL_SUCCESS != executeResultCode)
							{
								odbcPool->CleanUp(); // 해당 객체에 문제가 있다면 나머지를 모두 날린다.
								continue;
							}

							odbcPool->Release(std::move(connection));

							break; // 정상처리되었으므로 종료
						} while (true);

						std::this_thread::sleep_for(std::chrono::seconds(1));
					}

					m_odbcPoolTls.Destroy();
				}
			);
		}
	}

	void Put(std::shared_ptr<IQuery> task)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_queue.push(task);
	}

	bool TryPop(std::shared_ptr<IQuery>& task)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if (true == m_queue.empty())
		{
			return false;
		}

		task = m_queue.front();
		m_queue.pop();

		return true;
	}

	void Stop()
	{
		Put(nullptr);

		for (auto& t : m_threadGroup)
		{
			t.join();
		}
	}

private:
	OdbcPoolTls m_odbcPoolTls;

	_logging_ptr_t m_logging = std::make_shared<Logging>();

	std::mutex m_mutex;
	std::queue<std::shared_ptr<IQuery>> m_queue;

	std::vector<std::thread> m_threadGroup;
};

int main() 
{
	OdbcConfiguration config;
	config.connectionString = "Driver={ODBC Driver 17 for SQL Server};Server=tcp:172.31.101.38,1433;Database=MFR_GAME;Uid=MFRServerUser;Pwd=1234;language=english;ConnectRetryCount=0;";
	config.maxOdbcCount = 10;

	TlsWorkerThread twh(config);
	
	for (int32_t i = 0; i < 10; ++i)
	{
		int64_t usn = 1000121111200000002;
		std::string datetime = "2022-03-23 12:12:12";

		auto query = NamedQuery::CreateP_GAME_DAILY_ACHIEVEMENT_R();
		query->SetParameter(
			usn, 
			datetime
		);

		twh.Put(query);
	}
	
	twh.Run();

	/*
	// 1. ODBC 매니저 초기화 및 Connection 획득
	OdbcPool<NonThreadSafeQueue> odbcManager;

	// 사용자 정의 로깅 객체 생성
	_logging_ptr_t logging = std::make_shared<Logging>();
	odbcManager.AttachLogging(logging);
	odbcManager.Initialize("Driver={ODBC Driver 17 for SQL Server};Server=tcp:172.31.101.38,1433;Database=MFR_GAME;Uid=MFRServerUser;Pwd=1234;language=english;ConnectRetryCount=0;", 100);

	// 2. P_GAME_DAILY_ACHIEVEMENT_R 실행
	{
		do
		{
			int64_t usn = 1000121111200000002;
			std::string datetime = "2022-03-23 12:12:12";

			auto query = NamedQuery::CreateP_GAME_DAILY_ACHIEVEMENT_R();
			query->SetParameter(usn, datetime);

			auto connection = odbcManager.GetConnection();
			if (nullptr == connection)
			{
				// 네트웍 또는 장비 이슈로 인해 연결을 할수 없는 상태라면?? 

				break;
			}
			// 질의 정보를 등록
			connection->BindQuery(query);
			auto executeResultCode = connection->Execute();
			if (SQL_SUCCESS != executeResultCode)
			{
				odbcManager.CleanUp(); // 해당 객체에 문제가 있다면 나머지를 모두 날린다.
				continue;
			}

			odbcManager.Release(std::move(connection));

			break; // 정상처리되었으므로 종료
		} while (true);
	}

	// 3. P_GAME_LoginData_MARS_RU 실행
	{
		do
		{
			uint8_t loginMode = 0;
			int64_t usn = 0;
			std::string pid = "OTEST2020";
			int32_t serverID = 1001;
			std::string serverTime = "2022-03-23 12:12:12";
			std::string platform = "iOS";
			std::string country = "Kr";
			std::string languageCode = "Ko";

			auto query = NamedQuery::CreateP_GAME_LoginData_MARS_RU();
			query->SetParameter(loginMode, usn, pid, serverID, serverTime, platform, country, languageCode);

			auto connection = odbcManager.GetConnection();
			connection->BindQuery(query);
			auto executeResultCode = connection->Execute();
			if (SQL_SUCCESS != executeResultCode)
			{
				odbcManager.CleanUp(); // Thread Local로 사용될 경우(Single Thread)
				continue;
			}

			// 반환
			odbcManager.Release(std::move(connection));
			break;
		} while (true);
	}

	// 종료 처리
	odbcManager.Finalize();
	*/

	getchar();

	twh.Stop();

	return 0;
}
