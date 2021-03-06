#pragma once

#include "client.h"
#include "storage.h"

class magaya_client_storage : public dbs_client_storage
{
public:
	magaya_client_storage(stid_t sid, dbs_application* app);

protected:
	virtual boolean open(const char* server_connection_name, const char* login, const char* password, obj_storage* os) override;
};

