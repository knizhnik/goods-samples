#pragma once


class CGoodsSampleApp final
{
public:
	CGoodsSampleApp();

	boolean open(const char* dbs_name, char const* login = nullptr, char const* password = nullptr);
	void close();
	void run();
}; 