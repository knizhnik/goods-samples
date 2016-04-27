/////////////////////////////////////////////////////////////////
// Header file DBCmdEx.h

// Command added to GOODS database
//


#ifndef DBCmdEx_h
#define DBCmdEx_h

enum
{
	db_cmd_ex_start				= 200,  //The extra commands start at 200
	query_users_count			= 201,
	query_time					= 202,
	query_backup_status			= 203,	
	is_backup_running			= 204,	//Returns TRUE if there is a backup in progress
	is_valid_object				= 205,	//Returns TRUE if the object (OPID) is valid
	adjust_time					= 206,  //Adjusts time_t provided to the specific time provided (hour/minutes/seconds) using the local server time
	get_my_connection_string	= 207,	//Get connection string for current caller
	is_connection_string_valid	= 208,	//Validate connection sent against current connections
};

/*******************************************************************

  query_users_count
  Counts the amount of instances of the requesting Application that 
  have different HostName:UserName and their same Application ID

  The format of the name of the connection is as follows
  
  Ap:HostName:UserName:ProcessID:ThreadID
  
  Where:
  Ap = Application ID, two letters ID
  Host = Host name of the client computer
  User = Windows user logged in the computer
  ProcessID = ID of the process that established the connection
  ThreadID = ID of the thread that establised the connection

  The client sends one dbs_request with cmd = query_users_count
  The server responds with one dbs_request with cmd = query_users_count 
    and the amount of users in request.any.arg3

********************************************************************/


/*******************************************************************

  query_time
  Sends the time on the server computer to the client

  The client sends one dbs_request with cmd = query_time
  The server responds with one one dbs_request with cmd = query_users_count
    and the time in request.any.arg3 in time_t format

********************************************************************/


/*******************************************************************

  query_backup_status
  Sends the database backup status from server computer to the client

  The client sends one dbs_request with cmd = query_backup_status
  The server responds with one dbs_request with cmd = query_backup_status
    -the date of last successfull backup in request.sync.log_offs_low 
	 and request.sync.log_offs_high in time_t format
	-if there is not enough space to backup in request.sync.arg1
	-number of unsuccessful backups since the last backup in 
	 request.sync.arg2

********************************************************************/

//////////////////////////////////////////////////////////////////////////
//Refuse login codes 
enum refuse_login_codes {
	rlc_none					= 0,
	rlc_low_disk_space			= 1, // There is not enough disk space available
	rlc_authentication_required = 2,
	rlc_low_blob_disk_space		= 3,
};



#endif //DBCmdEx_h
