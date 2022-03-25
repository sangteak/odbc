﻿#include <windows.h>
#include "odbc.h"

// 여러 Row 읽기 처리
class P_GAME_DAILY_ACHIEVEMENT_R : public IDataAccessObject
{
public:
	struct element
	{
		int32_t type;
		std::string info;
		std::string expiretime;
	};

	virtual void HandleOdbcException(_odbc_error_ptr_t& err) override
	{
	}

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
	static Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>* CreateP_GAME_DAILY_ACHIEVEMENT_R()
	{
		return new Query<P_GAME_DAILY_ACHIEVEMENT_R, int64_t, std::string>(
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

int main() 
{	
	// ODBC 매니저 초기화
	OdbcManager odbcManager;
	odbcManager.Init("Driver={ODBC Driver 17 for SQL Server};Server=tcp:[ip],[port];Database=[dbtabase name];Uid=[id];Pwd=[pwd];language=english;ConnectRetryCount=0;", 10, 100);

	auto connection = odbcManager.GetConnection();

	// P_GAME_DAILY_ACHIEVEMENT_R 실행
	int64_t usn = 1000121111200000002;
	std::string datetime = "2022-03-23 12:12:12";

	auto query = NamedQuery::CreateP_GAME_DAILY_ACHIEVEMENT_R();
	query->SetParameter(usn, datetime);
	
	// 질의 정보를 등록
	connection->BindQuery(query);
	connection->Execute();

	// P_GAME_LoginData_MARS_RU 실행
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

	connection->BindQuery(query);
	connection->Execute();
	
	// 반환
	odbcManager.Release(connection);

	// 종료 처리
	odbcManager.CleanUp();

	return 0;
}