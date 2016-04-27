
#include "stdafx.h"
#include <atlutil.h>
#include "magaya_client_storage.h"


magaya_client_storage::magaya_client_storage(stid_t sid, dbs_application* app) 
	: dbs_client_storage(sid, app)
{
}

/*virtual oveerride*/
boolean magaya_client_storage::open(const char* server_connection_name, const char* login, const char* password)
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
	DWORD nSize = MAX_LOGIN_NAME;
	::GetUserNameA(user_name, &nSize);
	gethostname(host_name, MAX_LOGIN_NAME);

	char login_name[MAX_LOGIN_NAME];
	size_t login_name_len;

	if (login != NULL)
	{
		if (password == NULL) password = "";
		login_name_len = sprintf_s(login_name,
									"1\t%s\t%s\t%s\t%x\t%p\t%s\t%s",
									"el", host_name, user_name, GetCurrentProcessId(), task::current(), login, password);
	}
	else
	{
		login_name_len = sprintf_s(login_name,
									"0\t%s\t%s\t%s\t%x\t%p",
									"el", host_name, user_name, GetCurrentProcessId(), task::current());
	}

	const size_t snd_buf_size = sizeof(dbs_request) + login_name_len;
	snd_buf.put(snd_buf_size);

	dbs_request* login_req = (dbs_request*)&snd_buf;
	login_req->cmd = dbs_request::cmd_login;
	login_req->login.name_len = login_name_len;
	strcpy_s((char*)(login_req + 1), snd_buf_size, login_name);

	write(login_req, sizeof(dbs_request) + login_name_len);
	dbs_request rcv_req;
	read(&rcv_req, sizeof rcv_req);
	rcv_req.unpack();

	assert(rcv_req.cmd == dbs_request::cmd_ok ||
			rcv_req.cmd == dbs_request::cmd_bye ||
			rcv_req.cmd == dbs_request::cmd_refused);

	switch (rcv_req.cmd)
	{
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

	task::create(magaya_client_storage::receiver, this, task::pri_high);
	return True;
}
