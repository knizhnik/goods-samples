/////////////////////////////////////////////////////////////////////
// Implementation file ClientStorage.cpp

#include "stdafx.h"

#ifdef _WIN32
#include <atlutil.h>
#endif
#include "ClientStorage.h"
#include "mop.h"
#include "DBCmdEx.h"
#include "support.h"
//#include "resource.h"

#if defined(CS_SERVICE)
#include "..\CS\CSWinService.h"
extern CCSWinService *pCSApp;
#endif //CS_SERVICE

#include <vector>

//////////////////////////////////////////////////////////////////////
// Implementation for class CClientStorage

void task_proc CClientStorage::receiver(void* arg) 
{
	dbs_client_storage::receiver(arg);
} 

//Opens the database using server_name:port string
//
boolean CClientStorage::open(const char* server_connection_name, const char* login, const char* password) 
{
    sock = socket_t::connect(server_connection_name);
    if (!sock->is_ok()) 
	{ 
        msg_buf buf;
        sock->get_error_text(buf, sizeof buf);
        console::output("Failed to connect server '%s': %s\n", server_connection_name, buf);
        delete sock;
        sock = NULL; 
        return False;
    }

    opened = True;
    closing = False;
    waiting_reply = False;
    proceeding_invalidation = False;

    notify_buf.change_size(0);

	char host_name[MAX_LOGIN_NAME];
	char user_name[MAX_LOGIN_NAME];
#ifdef _WIN32
	DWORD nSize = MAX_LOGIN_NAME;
	::GetUserNameA(user_name, &nSize);
#else
	strcpy(user_name, getenv("USER"));
#endif
	gethostname(host_name, MAX_LOGIN_NAME);

	char login_name[MAX_LOGIN_NAME];
	size_t login_name_len;

    if (login != NULL) 
	{ 
        if (password == NULL) password = "";
		login_name_len = sprintf_s(login_name, 
								 "1\t%s\t%s\t%s\t%x\t%p\t%s\t%s", 
								 m_AppID, host_name, user_name, GetCurrentProcessId(), task::current(), login, password);
	}
	else
	{
		login_name_len = sprintf_s(login_name, 
								 "0\t%s\t%s\t%s\t%x\t%p", 
								 m_AppID, host_name, user_name, GetCurrentProcessId(), task::current());
	}
	
	const size_t snd_buf_size = sizeof(dbs_request) + login_name_len;
	tmp_buf.put(snd_buf_size);

    dbs_request* login_req = (dbs_request*)&tmp_buf;
    login_req->cmd = dbs_request::cmd_login;
    login_req->login.name_len = login_name_len;
	strcpy_s((char*)(login_req+1), snd_buf_size, login_name);

    write(login_req, sizeof(dbs_request) + login_name_len); 
    dbs_request rcv_req;
    read(&rcv_req, sizeof rcv_req);
    rcv_req.unpack();

    assert(rcv_req.cmd == dbs_request::cmd_ok ||
           rcv_req.cmd == dbs_request::cmd_bye ||
           rcv_req.cmd == dbs_request::cmd_refused);

    switch (rcv_req.cmd) 
	{ 
#if !defined(REMOTE)
		case dbs_request::cmd_ok:
			{
				// -- rcv_req.any.arg5 should have the major GOODS version part in the high word and the minor one in the low word
				if (rcv_req.any.arg5 != MAKELONG(nat4(((GOODS_VERSION) - nat4(GOODS_VERSION)) * 100), nat4(GOODS_VERSION)))
				{
					application->login_refused(id);
					return False;
				}
			}
			break;
#endif //!defined(REMOTE)

		case dbs_request::cmd_bye: 
			opened = False;
            application->disconnected(id);
            return False;

		case dbs_request::cmd_refused: 
            opened = False;
			application->receive_message(rcv_req.any.arg3);
            application->login_refused(id);
            return False;
    }   

    task::create(CClientStorage::receiver, this, task::pri_high); 
    return True;
}       

boolean CClientStorage::handle_communication_error()
{
#if !defined(CS_SERVICE)
#if (EXPEXPL || CS)

	TCHAR szMsg[256];
	::LoadString(GetModuleHandle(NULL), IDS_DB_CLOSED, szMsg, sizeof(szMsg));
	::MessageBox(NULL, szMsg, _T("Magaya Cargo System"), MB_OK | MB_ICONSTOP | MB_TASKMODAL | MB_SETFOREGROUND);		

#endif // (EXPEXPL || CS)

#else
	pCSApp->SetActionEvent();
#endif // CS_SERVICE

	_exit(EXIT_FAILURE);	
}

time_t CClientStorage::GetTime()
{
	dbs_request snd_req, rcv_req;
	snd_req.cmd = query_time;

	send_receive(&snd_req, 
				sizeof(snd_req), 
				rcv_req, 
				query_time);

	return cons_nat8(rcv_req.any.arg4, rcv_req.any.arg3);
}

time_t CClientStorage::AdjustUtcTimeToSpecificTime(time_t base_time, int hour, int minute, int second) 
{
    // adjust_time
 
    dbs_request snd_req, rcv_req;
    snd_req.cmd = adjust_time;
    snd_req.any.arg3 = nat8_low_part(base_time);
    snd_req.any.arg4 = nat8_high_part(base_time);
    snd_req.any.arg5 = second + minute * 60 + hour * 60 * 60;
 
    send_receive(&snd_req, sizeof(snd_req), rcv_req, snd_req.cmd);
 
    return cons_nat8(rcv_req.any.arg4, rcv_req.any.arg3);
}

std::string CClientStorage::GetCurrentConnectionString()
{
	dbs_request snd_req, rcv_req;
	snd_req.cmd = get_my_connection_string;

	dnm_buffer connection_string_buffer;
	send_receive(&snd_req, sizeof(snd_req), rcv_req, snd_req.cmd, connection_string_buffer);

	const auto connection_string_buffer_size = connection_string_buffer.size();
	std::vector<char> connection_string(connection_string_buffer_size + 1);
	memcpy(connection_string.data(), &connection_string_buffer, connection_string_buffer_size);
	connection_string[connection_string_buffer_size] = 0;

	return connection_string.data();
}

bool CClientStorage::ValidateConnectionString(std::string const& connection_string)
{
	const auto connection_string_length = nat4(connection_string.length());
	const auto request_size = sizeof(dbs_request);

	dbs_request snd_req, rcv_req;
	snd_req.cmd = is_connection_string_valid;
	snd_req.any.arg3 = connection_string_length;

	std::vector<BYTE> send_message_buffer(request_size + connection_string_length);
	memcpy(send_message_buffer.data(), &snd_req, request_size);
	memcpy(send_message_buffer.data() + request_size, connection_string.c_str(), connection_string_length);

	send_receive(send_message_buffer.data(), send_message_buffer.size(), rcv_req, snd_req.cmd);

	return rcv_req.any.arg1;
}

nat4 CClientStorage::GetCurrentUsersCount(char *app_id, nat4 &nInstances)
{
	dbs_request snd_req, rcv_req;
	snd_req.cmd = query_users_count;
	if (app_id == NULL)
		snd_req.any.arg4 = 0;
	else
		snd_req.any.arg4 = ((app_id[0] << 8) | app_id[1]) & 0x0000FFFF;

	send_receive(&snd_req, 
				sizeof(snd_req), 
				rcv_req, 
				query_users_count);
	nInstances = rcv_req.any.arg5;

	return rcv_req.any.arg3;
}

BOOL CClientStorage::OpidExists(opid_t opid)
{
	dbs_request snd_req;
	dbs_request rcv_req;

	snd_req.any.arg4 = opid;
    
	snd_req.cmd = is_valid_object;
	send_receive(&snd_req, sizeof(snd_req), rcv_req, is_valid_object);

	return BOOL(rcv_req.any.arg5);
}

BOOL CClientStorage::GetBackUpStatus(time_t &tt, int &hdd, int &errc)
{
	dbs_request snd_req, rcv_req;
	snd_req.cmd = query_backup_status;
	send_receive(&snd_req, 
				sizeof(snd_req), 
				rcv_req, 
				query_backup_status);
	if (rcv_req.cmd != query_backup_status)
		return FALSE;
	hdd = rcv_req.sync.arg1;
	errc = rcv_req.sync.arg2;
	tt = rcv_req.sync.log_offs_high;
	tt = (tt>>32) + rcv_req.sync.log_offs_low;
	return TRUE;
}

BOOL CClientStorage::IsBackupRunning()
{
	dbs_request snd_req;
	dbs_request rcv_req;
    
	snd_req.cmd = is_backup_running;
	send_receive(&snd_req, sizeof(snd_req), rcv_req, is_backup_running);
	if (rcv_req.cmd != is_backup_running)
		return FALSE;

	return BOOL(rcv_req.sync.arg1);
}


//////////////////////////////////////////////////////////////////////
// Implementation for class CObjectStorage

time_t CObjectStorage::GetTime()
{
	critical_section guard(cs);
	time_t t = ((CClientStorage*)storage)->GetTime();
	return t;
}

nat4 CObjectStorage::GetCurrentUsersCount(char *app_id, nat4 &nInstances)
{
	critical_section guard(cs);
	nat4 count = ((CClientStorage*)storage)->GetCurrentUsersCount(app_id, nInstances);
	return count;
}

BOOL CObjectStorage::OpidExists(opid_t opid)
{
	critical_section guard(cs);
	BOOL bOpidExits = ((CClientStorage*)storage)->OpidExists(opid);
	return bOpidExits;
}

BOOL CObjectStorage::GetBackUpStatus(time_t &tt, int &hdd, int &errc)
{
	critical_section guard(cs);
	return ((CClientStorage*)storage)->GetBackUpStatus(tt, hdd, errc);
}

BOOL CObjectStorage::IsBackupRunning()
{
	critical_section guard(cs);
	return static_cast<CClientStorage*>(storage)->IsBackupRunning();
}

time_t CObjectStorage::AdjustUtcTimeToSpecificTime(time_t base_time, int hour, int minute, int second)
{
    critical_section guard(cs);
    return static_cast<CClientStorage*>(storage)->AdjustUtcTimeToSpecificTime(base_time, hour, minute, second);
}

std::string CObjectStorage::GetCurrentConnectionString()
{
	critical_section guard(cs);
	return static_cast<CClientStorage*>(storage)->GetCurrentConnectionString();
}

bool CObjectStorage::ValidateConnectionString(std::string const& connection_string)
{
	critical_section guard(cs);
	return static_cast<CClientStorage*>(storage)->ValidateConnectionString(connection_string);
}

