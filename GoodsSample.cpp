// GoodsSample.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "GoodsSampleApp.h"

#ifdef _WIN32
static inline std::string to_string(const wchar_t* wide_chars_ptr)
{
	const std::wstring wstr(wide_chars_ptr);
	return { begin(wstr), end(wstr) };
}

int _tmain(int argc, _TCHAR* argv[])
#else

static inline std::string to_string(const char* chars_ptr)
{
	return std::string(chars_ptr);
}
int main(int argc, char* argv[])
#endif
{
	CGoodsSampleApp app;
	time_t start = time(NULL);
 
	if (argc < 1)
	{
		console::output("Invalid arguments...\n");
		return EXIT_FAILURE;
	}
	task::initialize(task::normal_stack);
	setlocale(LC_CTYPE, "ru_RU.utf8"); 

	std::string database_name = to_string(argv[1]);
	std::string login = argc > 2 ? to_string(argv[2]) : "";
	std::string password = argc > 3 ? to_string(argv[3]) : "";

	if (!app.open(database_name.c_str(), login.c_str(), password.c_str()))
	{
		console::output("Database not found\n");
		return EXIT_FAILURE;
	}

	app.run();
	app.close();

	console::output("overall time %d sec\n", time(NULL) - start);
#ifdef _WIN32
	system("pause");
#endif	
	return EXIT_SUCCESS;
}
