///////////////////////////////////////////////////////////////////////////
// Header file Storage.h

#ifndef STORAGE_H
#define STORAGE_H

#include "ClientStorage.h"
#include "DBCmdEx.h"

#include <string>

//Classes to override default goods implementation

// -- class CDBAttacher
//
class CDBAttacher
{
public:
	CDBAttacher(database& db) : _db(db) {
#ifdef _WIN32
		CoInitialize(NULL);
#endif
		_db.attach();
	}
	~CDBAttacher(void) {
		_db.detach();
#ifdef _WIN32
		CoUninitialize();
#endif
	}
private:
	database& _db;
};


//Database
class CDataBase : public database
{
public:
	CDataBase(const char *sAppID) 
		: m_LoginRefusedReason(rlc_none)
	{
		SetAppID(sAppID);
	}	
	void SetAppID(const char *sAppID){
		memcpy(m_AppID, sAppID, 2);
		m_AppID[2] = 0;
	}
	char *GetAppID(){
		return m_AppID;
	}	

	static unsigned char	m_ServerName[MAX_PATH + 1];
	static std::string	m_Username;
	static std::string	m_EncriptedPassword;

	static void CleanUp();

//implementation
	BOOL IsOpen(){
		return opened;
	}
    obj_storage *GetStorage(stid_t sid) {
		return get_storage(sid);
	}
	bool IsAuthenticationRequired() const {
		return m_LoginRefusedReason == rlc_authentication_required;
	}

    virtual dbs_storage* create_dbs_storage(stid_t sid) const;
#if !PGSQL_ORM
    virtual void disconnected(stid_t sid);
    virtual obj_storage* create_obj_storage(stid_t sid);
	virtual void receive_message( int message);

    BOOL Open(const char *servername, char const* login = NULL, char const* password = NULL);
	virtual void close() {
		detach();
		database::close();
	}
	
	virtual void login_refused(stid_t /*sid*/) { /* EMPTY BODY */ };


	time_t GetTime() const;
	nat4 GetCurrentUsersCount(char *app_id, nat4 &nInstances) const;
	BOOL IsOnlyOneConnectionActive() const;
	BOOL OpidExists(opid_t opid) const;
	BOOL GetBackUpStatus(time_t &tt, int &hdd, int &errc) const;
	BOOL IsBackupRunning() const;
	time_t AdjustUtcTimeToSpecificTime(time_t base_time, int hour, int minute, int second) const;
	std::string GetCurrentConnectionString() const;
	bool ValidateConnectionString(std::string const& connection_string) const;
 
	
	UINT GetLoginRefusedReasson() const {
		return m_LoginRefusedReason;
	}
#else
	
#endif

private:
	// -- do not allow to copy
	CDataBase(CDataBase const&);
	CDataBase& operator=(CDataBase const&);	

private:
	char	m_AppID[3];
	UINT	m_LoginRefusedReason;
};

//Instance of the database connection created in the entry point file of each project
//
#ifndef HYPERION
extern CDataBase DataBase;
#endif // HYPERION

#endif //STORAGE_H
