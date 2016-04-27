/////////////////////////////////////////////////////////////////////
// Header file ClientStorage.h

#ifndef CLIENTSTORAGE_H
#define CLIENTSTORAGE_H

#include "client.h"
#include <string>

class CClientStorage : public dbs_client_storage
{
	char m_AppID[3];

public:
	CClientStorage(const char *sAppID, stid_t sid, dbs_application* app)
		: dbs_client_storage(sid, app) {
		memcpy(m_AppID, sAppID, 2);
		m_AppID[2] = 0;
	}

	static void task_proc receiver(void* arg);
    virtual boolean open(const char* server_connection_name, const char* login, const char* password);
	virtual boolean handle_communication_error();

	time_t GetTime();
	nat4 GetCurrentUsersCount(char *app_id, nat4 &nInstances);
	BOOL OpidExists(opid_t opid);
	BOOL GetBackUpStatus(time_t &tt, int &hdd, int &errc);
	BOOL IsBackupRunning();

	time_t AdjustUtcTimeToSpecificTime(time_t base_time, int hour, int minute, int second);
	std::string GetCurrentConnectionString();
	bool ValidateConnectionString(std::string const& connection_string);
};


class CObjectStorage : public obj_storage
{
public:
    CObjectStorage(database* dbs, stid_t sid) 
		: obj_storage(dbs, sid) 
	{ 
		opened = False;
	}

	time_t GetTime();
	nat4 GetCurrentUsersCount(char *app_id, nat4 &nInstances);
	BOOL OpidExists(opid_t opid);
	BOOL GetBackUpStatus(time_t &tt, int &hdd, int &errc);
	BOOL IsBackupRunning();
	time_t AdjustUtcTimeToSpecificTime(time_t base_time, int hour, int minute, int second);
	std::string GetCurrentConnectionString();
	bool ValidateConnectionString(std::string const& connection_string);
};

#endif //CLIENTSTORAGE_H
