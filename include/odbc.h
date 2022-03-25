#pragma once

#include <string>
#include <deque>
#include <memory>
#include <sstream>
#include <sql.h>
#include <sqlext.h>
#include "msodbcsql.h"

#pragma comment(lib, "odbc32.lib")
#pragma comment(lib, "odbccp32.lib")

class OdbcError
{
public:
	enum class eErrorLevel
	{
		NoError = 0,
		Normal = 1,
		Critical = 2,
	};

	explicit OdbcError(SQLSMALLINT handleType, SQLHANDLE handle)
		: m_handleType(handleType)
		, m_handle(handle)
		, m_errorLevel(eErrorLevel::NoError)
	{
	}

	~OdbcError() = default;

	inline std::string_view GetState() { return m_state; }
	inline std::string_view GetMessage() { return m_message; }
	inline bool IsCritical() { return (eErrorLevel::Critical == m_errorLevel); }

	SQLRETURN Parse()
	{
		static constexpr SQLSMALLINT STATUS_RECORD_NUMBER = 1;  // 상태레코드 값 1 고정

		m_state.resize(16);
		m_message.resize(1024);
		SQLSMALLINT messageTextLength = static_cast<SQLSMALLINT>(m_message.length());
		SQLSMALLINT textLength = 0;

		do
		{
			auto sqlResultCode = SQLGetDiagRec(
				m_handleType,
				m_handle,
				STATUS_RECORD_NUMBER,
				reinterpret_cast<uint8_t*>(m_state.data()),
				NULL,
				reinterpret_cast<uint8_t*>(m_message.data()),
				messageTextLength,
				&textLength);
			// 
			if (SQL_SUCCESS_WITH_INFO == sqlResultCode && m_message.length() <= textLength)
			{
				// 2배 증가
				m_state = "";
				m_message = "";
				messageTextLength <<= 1;
				m_message.resize(messageTextLength);
				continue;
			}

			if (SQL_SUCCESS != sqlResultCode)
			{
				return sqlResultCode;
			}

			break;
		} while (true);

		m_errorLevel = JudgeErrorLevelAndGet();

		return SQL_SUCCESS;
	}

	std::string ToString()
	{
		std::stringstream ss;

		ss << "SQLState:" << m_state;
		ss << ", MessageText:" << m_message;

		return ss.str();
	}

private:
	eErrorLevel JudgeErrorLevelAndGet()
	{
		if (0 == m_state.compare("08S01")		// communication link failure.
			|| 0 == m_state.compare("08S02")	// Physical connection is not usable.
			|| 0 == m_state.compare("HY000"))
		{
			return eErrorLevel::Critical;
		}

		return eErrorLevel::Normal;
	}

	SQLSMALLINT m_handleType = 0;
	SQLHANDLE m_handle = SQL_NULL_HANDLE;
	eErrorLevel m_errorLevel;
	std::string m_state;
	std::string m_message;
};
using _odbc_error_ptr_t = std::shared_ptr<OdbcError>;

class StatementException
{
public:
	StatementException(_odbc_error_ptr_t error)
		: m_error(error)
	{
	}

	inline _odbc_error_ptr_t& GetNative() { return m_error; }
	inline std::string_view GetState() { return m_error->GetState(); }
	inline std::string_view GetMessage() { return m_error->GetMessage(); }

private:
	_odbc_error_ptr_t m_error;
};

template <typename T>
struct SqlTypes {};

template <>
struct SqlTypes<char>
{
	static constexpr int16_t C_TYPE = SQL_C_CHAR;
	static constexpr int16_t SQL_TYPE = SQL_CHAR;
};

template <>
struct SqlTypes<wchar_t>
{
	static constexpr int16_t C_TYPE = SQL_C_WCHAR;
	static constexpr int16_t SQL_TYPE = SQL_WCHAR;
};

template <>
struct SqlTypes<int8_t>
{
	static constexpr int16_t C_TYPE = SQL_C_STINYINT;
	static constexpr int16_t SQL_TYPE = SQL_TINYINT;
};

template <>
struct SqlTypes<uint8_t>
{
	static constexpr int16_t C_TYPE = SQL_C_UTINYINT;
	static constexpr int16_t SQL_TYPE = SQL_TINYINT;
};

template <>
struct SqlTypes<bool>
{
	static constexpr int16_t C_TYPE = SQL_C_BIT;
	static constexpr int16_t SQL_TYPE = SQL_BIT;
};

template <>
struct SqlTypes<int16_t>
{
	static constexpr int16_t C_TYPE = SQL_C_SSHORT;
	static constexpr int16_t SQL_TYPE = SQL_SMALLINT;
};

template <>
struct SqlTypes<uint16_t>
{
	static constexpr int16_t C_TYPE = SQL_C_USHORT;
	static constexpr int16_t SQL_TYPE = SQL_SMALLINT;
};

template <>
struct SqlTypes<int32_t>
{
	static constexpr int16_t C_TYPE = SQL_C_SLONG;
	static constexpr int16_t SQL_TYPE = SQL_INTEGER;
};

template <>
struct SqlTypes<uint32_t>
{
	static constexpr int16_t C_TYPE = SQL_C_ULONG;
	static constexpr int16_t SQL_TYPE = SQL_INTEGER;
};

template <>
struct SqlTypes<int64_t>
{
	static constexpr int16_t C_TYPE = SQL_C_SBIGINT;
	static constexpr int16_t SQL_TYPE = SQL_BIGINT;
};

template <>
struct SqlTypes<uint64_t>
{
	static constexpr int16_t C_TYPE = SQL_C_UBIGINT;
	static constexpr int16_t SQL_TYPE = SQL_BIGINT;
};

template <>
struct SqlTypes<long>
{
	static constexpr int16_t C_TYPE = SQL_C_SLONG;
	static constexpr int16_t SQL_TYPE = SQL_INTEGER;
};

template <>
struct SqlTypes<unsigned long>
{
	static constexpr int16_t C_TYPE = SQL_C_ULONG;
	static constexpr int16_t SQL_TYPE = SQL_INTEGER;
};

template <>
struct SqlTypes<float>
{
	static constexpr int16_t C_TYPE = SQL_C_FLOAT;
	static constexpr int16_t SQL_TYPE = SQL_FLOAT;
};

template <>
struct SqlTypes<double>
{
	static constexpr int16_t CTYPE = SQL_C_DOUBLE;
	static constexpr int16_t SQL_TYPE = SQL_DOUBLE;
};

class Statement
{
public:
	enum class eFetchResult
	{
		OK = 0,
		ERR = 1,
		EMPTY = 2,
		CLOSE = 3
	};

	inline bool IsOpen()
	{
		return (nullptr != m_hStmt);
	}

	inline void Open(SQLHSTMT hStmt)
	{
		m_hStmt = hStmt;
	}

	void Close()
	{
		m_index_read = 0;
		m_index_param = 0;
		m_index_recordset = 0;
		m_fetchResult = eFetchResult::CLOSE;

		// close cursor.
		SQLCloseCursor(m_hStmt);

		// execute result close. (SQL_CLOSE, SQL_DROP, SQL_UNBIND, SQL_RESET_PARAMS)
		SQLFreeStmt(m_hStmt, SQL_CLOSE);
	}

	void Destroy()
	{
		if (false == IsOpen())
		{
			return;
		}

		SQLFreeHandle(SQL_HANDLE_STMT, m_hStmt);
		m_hStmt = SQL_NULL_HANDLE;
	}

	inline int GetRecordsetIndex() { return m_index_recordset; }

	inline eFetchResult GetFetchResult() { return m_fetchResult; }

	inline bool IsNoData() { return (eFetchResult::EMPTY == m_fetchResult); }

	inline bool Ok() { return false; }

	SQLRETURN Prepare(const char* statementText)
	{
		return SQLPrepare(m_hStmt, (SQLCHAR*)statementText, SQL_NTS);
	}

	SQLRETURN Execute()
	{
		return SQLExecute(m_hStmt);
	}

	bool MoveNext()
	{
		auto retCode = Fetch();
		if (retCode == SQL_SUCCESS || retCode == SQL_SUCCESS_WITH_INFO)
		{
			return true;
		}

		return false;
	}

	bool MoveNextRecordSet()
	{
		SQLRETURN retcode = SQLMoreResults(m_hStmt);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			retcode = Fetch();
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_NO_DATA)
			{
				++m_index_recordset;
				return true;
			}
		}

		return false;
	}

	SQLRETURN Fetch()
	{
		SQLRETURN retcode = SQL_ERROR;

		m_fetchResult = eFetchResult::ERR;

		SQLSMALLINT column_count = 0;
		retcode = SQLNumResultCols(m_hStmt, &column_count);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_NO_DATA)
		{
			if (column_count == 0)
			{
				m_fetchResult = eFetchResult::EMPTY;
				return SQL_NO_DATA;
			}
		}

		retcode = SQLFetch(m_hStmt);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO || retcode == SQL_NO_DATA)
		{
			m_index_read = 0;
			m_fetchResult = eFetchResult::OK;
			if (SQL_NO_DATA == retcode)
			{
				m_fetchResult = eFetchResult::EMPTY;
			}
		}

		return retcode;
	}

	template <typename T>
	bool AddParam(const T& value)
	{
		return (SQLBindParameter
		(
			m_hStmt,
			++m_index_param,
			SQL_PARAM_INPUT,
			SqlTypes<T>::C_TYPE,
			SqlTypes<T>::SQL_TYPE,
			sizeof(T),
			0,
			const_cast<T*>(&value),
			0,
			0
		) == SQL_SUCCESS);
	}

	bool AddParam(const std::string& value)
	{
		SQLLEN len = static_cast<SQLLEN>(value.length());
		if (SQLBindParameter(m_hStmt, ++m_index_param, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, len, 0, const_cast<char*>(value.c_str()), len, 0) == SQL_SUCCESS)
			return true;
		return false;
	}

	bool AddParam(const char* value, int32_t len)
	{
		if (SQLBindParameter(m_hStmt, ++m_index_param, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, len, 0, const_cast<char*>(value), len, 0) == SQL_SUCCESS)
			return true;
		return false;
	}

	bool AddParam(const wchar_t* value, int32_t len)
	{
		if (SQLBindParameter(m_hStmt, ++m_index_param, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_VARCHAR, len, 0, const_cast<wchar_t*>(value), len, 0) == SQL_SUCCESS)
			return true;
		return false;
	}

	bool AddParam_Binary(const char* value, int32_t len)
	{
		if (SQLBindParameter(m_hStmt, ++m_index_param, SQL_PARAM_INPUT, SQL_C_BINARY, SQL_VARBINARY, 0, 0, const_cast<char*>(value), len, 0) == SQL_SUCCESS)
			return true;
		return false;
	}

	template <typename T>
	void ReadData(T& value)
	{
		if (SQL_SUCCESS != SQLGetData(m_hStmt, ++m_index_read, SqlTypes<T>::C_TYPE, static_cast<SQLPOINTER>(&value), sizeof(T), nullptr))
		{
			throw StatementException(GetError());
		}
	}

	void ReadData(char* data, int32_t len)
	{
		if (SQL_SUCCESS != SQLGetData(m_hStmt, ++m_index_read, SQL_C_CHAR, static_cast<SQLPOINTER>(data), len, nullptr))
		{
			throw StatementException(GetError());
		}
	}

	void ReadData(wchar_t* data, int32_t len)
	{
		if (SQL_SUCCESS != SQLGetData(m_hStmt, ++m_index_read, SQL_C_WCHAR, static_cast<SQLPOINTER>(data), len, nullptr))
		{
			throw StatementException(GetError());
		}
	}

	void ReadData_TimeStamp(struct tm& timeinfo)
	{
		TIMESTAMP_STRUCT ts;
		//SQLLEN len;
		if (SQL_SUCCESS != SQLGetData(m_hStmt, ++m_index_read, SQL_C_TYPE_TIMESTAMP, static_cast<SQLPOINTER>(&ts), sizeof(TIMESTAMP_STRUCT), nullptr))
		{
			throw StatementException(GetError());
		}

		timeinfo.tm_sec = ts.second;   // seconds after the minute - [0, 60] including leap second
		timeinfo.tm_min = ts.minute;   // minutes after the hour - [0, 59]
		timeinfo.tm_hour = ts.hour;  // hours since midnight - [0, 23]
		timeinfo.tm_mday = ts.day;  // day of the month - [1, 31]
		timeinfo.tm_mon = ts.month;   // months since January - [0, 11]
		timeinfo.tm_year = ts.year;  // years since 1900
		timeinfo.tm_isdst = -1; // daylight savings time flag
	}

	void ReadData(std::string& out_value)
	{
		auto columnNumber = ++m_index_read;
		int32_t len = (GetDataSize(columnNumber) * sizeof(std::string::value_type)) + 1;
		if (len <= 1)
			return;

		out_value.resize(len);

		SQLLEN temp = static_cast<SQLLEN>(len);
		SQLRETURN error = SQLGetData(m_hStmt, columnNumber, SqlTypes<char>::C_TYPE, out_value.data(), out_value.length(), &temp);
		if (!(error == SQL_SUCCESS || error == SQL_SUCCESS_WITH_INFO))
		{
			throw StatementException(GetError());
		}
	}

	void ReadData(std::wstring& out_value)
	{
		auto columnNumber = ++m_index_read;
		int32_t len = (GetDataSize(columnNumber) * sizeof(std::wstring::value_type)) + 1;
		if (len <= 1)
			return;

		out_value.resize(len);

		SQLLEN temp = static_cast<SQLLEN>(len);
		SQLRETURN error = SQLGetData(m_hStmt, columnNumber, SqlTypes<wchar_t>::C_TYPE, out_value.data(), out_value.length(), &temp);
		if (!(error == SQL_SUCCESS || error == SQL_SUCCESS_WITH_INFO))
		{
			throw StatementException(GetError());
		}
	}

	void ReadData_Binary(std::string& out_value)
	{
		auto columnNumber = ++m_index_read;
		int32_t len = (GetDataSize(columnNumber) * sizeof(std::wstring::value_type)) + 1;
		if (len <= 1)
			return;

		out_value.resize(len);

		SQLLEN temp = static_cast<SQLLEN>(len);
		SQLRETURN error = SQLGetData(m_hStmt, columnNumber, SQL_C_BINARY, out_value.data(), out_value.length(), &temp);
		if (!(error == SQL_SUCCESS || error == SQL_SUCCESS_WITH_INFO))
		{
			throw StatementException(GetError());
		}
	}

	void ReadData_Binary(void* out_value, int32_t len, int32_t& out_len)
	{
		out_len = GetDataSize(++m_index_read);
		if (out_len == 0)
			return;

		if (out_value == nullptr)
		{
			out_value = new char[out_len];
			memset(out_value, 0x00, out_len);
		}

		SQLRETURN error = SQLGetData(m_hStmt, m_index_read, SQL_C_BINARY, static_cast<SQLPOINTER>(out_value), out_len, nullptr);
		if (!(error == SQL_SUCCESS || error == SQL_SUCCESS_WITH_INFO))
		{
			throw StatementException(GetError());
		}
	}

	int32_t GetDataSize(int32_t col_num)
	{
		// type = SQL_C_BINARY, SQL_C_CHAR, SQL_C_WCHAR
		SQLCHAR colname[128];
		SQLSMALLINT colnamelen;
		SQLSMALLINT coltype = 0;
		SQLULEN collen = 0;
		SQLSMALLINT decimaldigits;
		SQLSMALLINT nullable;
		SQLRETURN ret = SQLDescribeCol(m_hStmt, col_num, colname, sizeof(colname), &colnamelen, &coltype, &collen, &decimaldigits, &nullable);
		if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO)
			return static_cast<int32_t>(collen);

		return -1;
	}

	_odbc_error_ptr_t GetError()
	{
		auto odbcError = std::make_shared<OdbcError>(SQL_HANDLE_STMT, m_hStmt);
		odbcError->Parse();

		return odbcError;
	}

private:
	SQLHSTMT m_hStmt = SQL_NULL_HSTMT;
	eFetchResult m_fetchResult;
	SQLUSMALLINT m_index_read = 0;
	SQLUSMALLINT m_index_param = 0;
	int m_index_recordset = 0;
};

class IDataAccessObject
{
public:
	// 실행 과정에서 에러가 발생할 경우 내용을 공유 합니다.
	virtual void HandleOdbcException(_odbc_error_ptr_t& err) = 0;

	// 레코드 셋 데이터를 읽기 가능한 상태로 전달한다.
	virtual bool Parse(Statement* hstmt) = 0;

	// 처리 끝! 결과 처리
	virtual void Process() = 0;
};

class IQuery
{
public:
	virtual bool Build(Statement* statement) = 0;

	virtual const char* GetScript() = 0;
	virtual IDataAccessObject* GetDao() = 0;
};

// DB 처리를 위한 스크립트 및 속성 값 관리 객체
template <typename DAO, typename... Args>
class Query : public IQuery
{
public:
	explicit Query(std::string_view str)
		: m_dao(new DAO)
	{
		m_query = str;
	}

	~Query() = default;

	virtual const char* GetScript() override
	{
		return m_query.c_str();
	}

	virtual IDataAccessObject* GetDao() override
	{
		return m_dao.get();
	}

	void SetParameter(Args... args)
	{
		m_parameters = std::make_tuple(args...);
	}

	virtual bool Build(Statement* statement) override
	{
		if (nullptr == statement)
		{
			return false;
		}

		m_statement = statement;

		MakeParameters(m_parameters, std::index_sequence_for<Args...>{});

		return true;
	}

private:
	template <typename Tuple, std::size_t... Is>
	void MakeParameters(const Tuple& t, std::index_sequence<Is...>)
	{
		(
			m_statement->AddParam(std::get<Is>(t)),
			...
			);

	}

	Statement* m_statement;

	std::string m_query;
	std::tuple<Args...> m_parameters;

	std::unique_ptr<IDataAccessObject> m_dao;
};

// DB 연결 객체
class Odbc
{
public:
	Odbc()
	{
	}

	~Odbc()
	{
		CleanUp();
	}

	bool Setup(const char* connectionString)
	{
		m_hEnv = AllocENV();
		if (m_hEnv == nullptr)
		{
			return false;
		}

		m_hDbc = AllocDBC(m_hEnv, connectionString);
		if (m_hDbc == nullptr)
		{
			if (m_hEnv != nullptr)
			{
				SQLFreeHandle(SQL_HANDLE_STMT, m_hEnv);
				m_hEnv = nullptr;
			}

			return false;
		}

		m_statement.Open(AllocSTMT(m_hDbc));

		return m_statement.IsOpen();
	}

	void CleanUp()
	{
		if (true == m_statement.IsOpen())
		{
			m_statement.Destroy();
		}

		if (m_hDbc != nullptr)
		{
			SQLDisconnect(m_hDbc);
			SQLFreeHandle(SQL_HANDLE_DBC, m_hDbc);
			m_hDbc = SQL_NULL_HANDLE;
		}

		if (m_hEnv != nullptr)
		{
			SQLFreeHandle(SQL_HANDLE_ENV, m_hEnv);
			m_hEnv = SQL_NULL_HANDLE;
		}
	}

	inline Statement& GetStatement()
	{
		return m_statement;
	}

	bool BindQuery(IQuery* query)
	{
		m_query = query;

		// 바인딩 합니다.
		m_query->Build(&GetStatement());

		return true;
	}

	SQLRETURN Execute()
	{
		auto sqlResultCode = GetStatement().Prepare(m_query->GetScript());
		if (SQL_SUCCESS != sqlResultCode)
		{
			auto errorObject = GetStatement().GetError();
			m_query->GetDao()->HandleOdbcException(errorObject);

			return sqlResultCode;
		}

		sqlResultCode = GetStatement().Execute();
		if (SQL_SUCCESS != sqlResultCode)
		{
			// 풀에 반환되지 않도록 처리되어야하며 로직에 현재 상황이 보고되어야 한다.
			auto errorObject = GetStatement().GetError();
			m_query->GetDao()->HandleOdbcException(errorObject);

			return sqlResultCode;
		}

		sqlResultCode = GetStatement().Fetch();
		if (SQL_SUCCESS != sqlResultCode)
		{
			// 풀에 반환되지 않도록 처리되어야하며 로직에 현재 상황이 보고되어야 한다.
			if (SQL_NO_DATA != sqlResultCode)
			{
				auto errorObject = GetStatement().GetError();
				m_query->GetDao()->HandleOdbcException(errorObject);
			}
			return sqlResultCode;
		}

		try
		{
			do
			{
				if (true == GetStatement().IsNoData())
				{
					// 해당 레코드셋에 데이터가 없을 경우 다음 레코드셋으로 이동합니다.
					continue;
				}

				if (false == m_query->GetDao()->Parse(&GetStatement()))
				{
					// Todo: 예외발생하였으며 후 처리가 필요하다.
					// 연결의 문제가 아닌 로직의 문제이기 때문에 break 후 SQL_SUCCESS를 반환한다.
					break;
				}
			} while (true == GetStatement().MoveNextRecordSet());

			// 레코드셋 파싱이 끝난 시점에 결과 처리를 할수 있도록 Result 메서드를 호출 한다.
			m_query->GetDao()->Process();

		}
		catch (StatementException& e)
		{
			// 크리티컬일 경우 에러를 반환한다.
			if (true == e.GetNative()->IsCritical())
			{
				return SQL_ERROR;
			}

			m_query->GetDao()->HandleOdbcException(e.GetNative());
		}

		GetStatement().Close();

		return SQL_SUCCESS;
	}

private:
	SQLHENV AllocENV()
	{
		// ms. https://docs.microsoft.com/ko-kr/sql/odbc/microsoft-open-database-connectivity-odbc?view=sql-server-2017
		// ibm. https://www.ibm.com/support/knowledgecenter/ko/SSEPGG_10.5.0/com.ibm.db2.luw.apdv.cli.doc/doc/r0000553.html

		SQLHENV hEnv;
		SQLRETURN retcode = SQL_ERROR;

		// Allocate an environment.
		retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			return nullptr;
		}

		// https://www.ibm.com/support/knowledgecenter/ko/SSEPGG_10.5.0/com.ibm.db2.luw.apdv.cli.doc/doc/r0006817.html

		// Register this as an application that expects 3.x behavior, you must register something if you use AllocHandle.
		retcode = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			SQLFreeHandle(SQL_HANDLE_STMT, hEnv);
			return nullptr;
		}

		return hEnv;
	}

	SQLHDBC AllocDBC(SQLHENV hEnv, const char* connection_string)
	{
		SQLHDBC hDbc = nullptr;
		SQLRETURN retcode = SQL_ERROR;

		// Allocate a connection.
		retcode = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			return nullptr;
		}

		// connection attribute setting.
		retcode = SQLSetConnectAttr(hDbc, SQL_LOGIN_TIMEOUT, reinterpret_cast<SQLPOINTER>(1), 0);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			SQLFreeHandle(SQL_HANDLE_STMT, hDbc);
			return nullptr;
		}

		retcode = SQLSetConnectAttr(hDbc, SQL_ATTR_CONNECTION_TIMEOUT, reinterpret_cast<SQLPOINTER>(1), 0);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			SQLFreeHandle(SQL_HANDLE_STMT, hDbc);
			return nullptr;
		}

		retcode = SQLSetConnectAttr(hDbc, SQL_ATTR_AUTOCOMMIT, reinterpret_cast<SQLPOINTER>(SQL_AUTOCOMMIT_ON), SQL_IS_UINTEGER);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			SQLFreeHandle(SQL_HANDLE_STMT, hDbc);
			return nullptr;
		}
#ifdef SQL_COPT_SS_MARS_ENABLED
		retcode = SQLSetConnectAttr(hDbc, SQL_COPT_SS_MARS_ENABLED, reinterpret_cast<SQLPOINTER>(SQL_MARS_ENABLED_YES), SQL_IS_UINTEGER);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			SQLFreeHandle(SQL_HANDLE_STMT, hDbc);
			return nullptr;
		}
#endif

		SQLCHAR* ODBC_ConnectionString = (SQLCHAR*)connection_string;
		SQLCHAR         buffer[1024] = { 0x00, };
		SQLSMALLINT     outlen = 0;

		// https://docs.microsoft.com/ko-kr/sql/odbc/reference/syntax/sqldriverconnect-function?view=sql-server-ver15
		// Connect to the driver.  Use the connection string if supplied on the input, otherwise let the driver manager prompt for input.
		retcode = SQLDriverConnect(hDbc, nullptr, ODBC_ConnectionString, SQL_NTS, buffer, static_cast<SQLSMALLINT>(255), &outlen, SQL_DRIVER_NOPROMPT);
		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
		{
			return hDbc;
		}

		// 		int32_t retry_count = RETRY_COUNT;
		// 		retcode = SQLGetConnectAttr(hDbc, SQL_COPT_SS_CONNECT_RETRY_COUNT, &retry_count, SQL_IS_INTEGER, NULL);
		// 		retcode = SQLGetConnectAttr(hDbc, SQL_COPT_SS_CONNECT_RETRY_INTERVAL, &retry_count, SQL_IS_INTEGER, NULL);

		std::string error;
		std::string sqlState;
		long out_native_error = 0L;
		//GetErrorString(SQL_HANDLE_DBC, hDbc, sqlState, error, out_native_error);

		SQLFreeHandle(SQL_HANDLE_STMT, hDbc);
		return nullptr;
	}

	SQLHSTMT AllocSTMT(SQLHDBC hDbc)
	{
		SQLHSTMT hStmt;
		SQLRETURN retcode = SQL_ERROR;

		// Allocate a Statement.
		retcode = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
		if (!(retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO))
		{
			return nullptr;
		}

		return hStmt;
	}

	SQLHENV m_hEnv;
	SQLHDBC m_hDbc;
	Statement m_statement;

	IQuery* m_query;
};

// DB 연결 객체를 관리합니다.(ODBC Pool)
class OdbcManager
{
public:
	using _odbc_range_t = std::pair<int32_t, int32_t>;

	bool Init(const char* connectionString, int32_t minOdbc, int32_t maxOdbc)
	{
		m_connectionString = connectionString;

		if (maxOdbc < minOdbc)
			return false;

		m_odbcRange = std::make_pair(minOdbc, maxOdbc);

		return true;
	}

	void CleanUp()
	{
		// 현재 사용하지 않고 있는 것들...종료 처리
	}

	std::shared_ptr<Odbc> GetConnection()
	{
		std::shared_ptr<Odbc> odbc;
		if (true == m_pool.empty())
		{
			odbc.reset(new Odbc);

			if (false == odbc->Setup(m_connectionString.c_str()))
			{
				return nullptr;
			}
		}
		else
		{
			odbc = m_pool.front();
			m_pool.pop_front();
		}

		return odbc;
	}

	void Release(std::shared_ptr<Odbc>& odbc)
	{
		//m_pool.push(std::move(odbc));
		m_pool.push_front(std::move(odbc));
	}

private:
	std::string m_connectionString;
	_odbc_range_t m_odbcRange;

	// required thread-safe queue
	//tbb::concurrent_bounded_queue<std::shared_ptr<Odbc>> m_pool;
	std::deque<std::shared_ptr<Odbc>> m_pool;
};