////////////////////////////////////////////////////////////////////////////
// Implementation file Storage.cpp

#include "StdAfx.h"

#include <atlutil.h>
#include "Storage.h"
#include "DBCmdEx.h"
//#include "resource.h"
//#include "interval.h"
//#include "AppIDs.h"

#if defined(CS_SERVICE)
#include "..\CS\CSWinService.h"
extern CCSWinService *pCSApp;
#endif //CS_SERVICE

////////////////////////////////////////////////////////////////////////////
// Implementation for class CDataBase

unsigned char CDataBase::m_ServerName[];
std::string	CDataBase::m_Username;
std::string	CDataBase::m_EncriptedPassword;


void CDataBase::disconnected(stid_t sid)
{
#if !defined(CS_SERVICE)
#if (EXPEXPL || CS)
	TCHAR szMsg[256];
	::LoadString(GetModuleHandle(NULL), IDS_DB_CLOSED, szMsg, sizeof(szMsg));
	::MessageBox(NULL, szMsg, _T("Magaya Cargo System"), MB_OK | MB_ICONSTOP | MB_TASKMODAL | MB_SETFOREGROUND);		
#endif //(EXPEXPL || CS)
#endif // CS_SERVICE

#if defined(CS_SERVICE)
	pCSApp->SetActionEvent();
#endif // CS_SERVICE

	_exit(EXIT_FAILURE);
}


BOOL CDataBase::Open(const char *servername, char const* login /*= NULL*/, char const* password /*= NULL*/)
{
    critical_section guard(cs);

	if(opened)
		return True;

	m_LoginRefusedReason = rlc_none;
	//if server name is the same as localhost try connecting using the
	//loop back 127.0.0.1
	char host_name[MAX_PATH+1];
	char svr_name[MAX_PATH+1];
	char name[MAX_PATH+1];
	memset(host_name, 0, sizeof(host_name));
	memset(svr_name, 0, sizeof(svr_name));
	memset(name, 0, sizeof(name));
	strcpy_s(name, MAX_PATH + 1, servername);

	const char *pEnd = strchr(servername, ':');
	strncpy_s(svr_name, MAX_PATH + 1, servername, pEnd - servername);
	int port = atoi(pEnd+1);
	gethostname(host_name, MAX_PATH);
	if (_stricmp(host_name, svr_name) == 0)
	{
		sprintf_s(name, "127.0.0.1:%d", port);
	}
	n_storages = 1;
    storages = NEW obj_storage*[n_storages];
    memset(storages, 0, n_storages * sizeof(obj_storage*));

	storages[0] = NEW CObjectStorage(this, 0);


	if (login == NULL)
	{
		if (!storages[0]->open(name))
			if (m_LoginRefusedReason == rlc_none)
				storages[0]->open(servername);
	}		
	else
	{
		if (!storages[0]->open(name, login, password))			
		{ 
			if (m_LoginRefusedReason == rlc_none)
				 storages[0]->open(servername, login, password);
		}				

		// -- authentication was required
		m_LoginRefusedReason = rlc_authentication_required;
	}

	if (!storages[0]->is_opened())
	{
		database::close();
		return False;
	}

    opened = True;
	strncpy_s((char*)m_ServerName, MAX_PATH + 1, servername, MAX_PATH);
	m_Username = login == NULL ? "" : login;
	m_EncriptedPassword = password == NULL ? "" : password;
	attach();

	return True;
}

dbs_storage* CDataBase::create_dbs_storage(stid_t sid) const 
{
	return NEW CClientStorage(m_AppID, sid, storages[sid]);
}

obj_storage* CDataBase::create_obj_storage(stid_t sid)
{
    return NEW CObjectStorage(this, sid);
}

time_t CDataBase::GetTime() const
{
	return ((CObjectStorage*)storages[0])->GetTime();
}

nat4 CDataBase::GetCurrentUsersCount(char *app_id, nat4 &nInstances) const
{
	return ((CObjectStorage*)storages[0])->GetCurrentUsersCount(app_id, nInstances);
}

/*
BOOL CDataBase::IsOnlyOneConnectionActive() const
{
	nat4 nInstances = 0;
	nat4 nUsers = GetCurrentUsersCount(ANY_ID, nInstances);
	return nUsers == 1 && nInstances == 1;
}
*/

BOOL CDataBase::OpidExists(opid_t opid) const
{
	return ((CObjectStorage*)storages[0])->OpidExists(opid);
}

BOOL CDataBase::GetBackUpStatus(time_t &tt, int &hdd, int &errc) const
{
	return ((CObjectStorage*)storages[0])->GetBackUpStatus(tt, hdd, errc);
}

BOOL CDataBase::IsBackupRunning() const
{
	return static_cast<CObjectStorage*>(storages[0])->IsBackupRunning();
}

time_t CDataBase::AdjustUtcTimeToSpecificTime(time_t base_time, int hour, int minute, int second) const
{
    return static_cast<CObjectStorage*>(storages[0])->AdjustUtcTimeToSpecificTime(base_time, hour, minute, second);
}
 
std::string CDataBase::GetCurrentConnectionString() const
{
	return static_cast<CObjectStorage*>(storages[0])->GetCurrentConnectionString();
}

bool CDataBase::ValidateConnectionString(std::string const& connection_string) const
{
	return static_cast<CObjectStorage*>(storages[0])->ValidateConnectionString(connection_string);
}

/*
// returns TRUE if the backup finished or is not going and the calling process can continue
// returns FALSE if the calling thead must finished b/c the process is being halted
BOOL CDataBase::WaitForBackupIsFinished(HANDLE m_hEndEvent) const
{
	while (IsBackupRunning())	// stop the process while a backup is in progress
	{
		if (WaitForSingleObject(m_hEndEvent, Interval<IntervalUnit::second, 30>::value) == WAIT_OBJECT_0)
			return FALSE;
	}
	return TRUE;
}
*/
void CDataBase::receive_message(int message)
{
	m_LoginRefusedReason = message;
	switch (message)
	{
		case rlc_low_disk_space:
#if !defined(CS_SERVICE) && (defined(EXPEXPL) || defined(CS))
			TCHAR szMsg[256];
			::LoadString(GetModuleHandle(NULL), IDS_DB_LOW_SPACE, szMsg, sizeof(szMsg));
			::MessageBox(NULL, szMsg, _T("Magaya Cargo System"), MB_OK | MB_ICONSTOP);		
#endif 
			break;

		default:
			break;
	}
}

void CDataBase::CleanUp()
{
    object_handle::cleanup_object_pool();
}
