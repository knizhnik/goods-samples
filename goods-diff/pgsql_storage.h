// -*- mode: c++; tab-width: 8; c-basic-offset: 4 -*-
//-< STORAGE.H >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jan-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 23-Apr-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Abstract database storage interface.  
//-------------------------------------------------------------------*--------*

#ifndef __PGSQL_STORAGE_H__
#define __PGSQL_STORAGE_H__

#include "goods.h"
#include "dbscls.h"

#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/prepared_statement>
#include <pqxx/except>
#include <pqxx/binarystring>

using namespace pqxx;
using namespace pqxx::prepare;

BEGIN_GOODS_NAMESPACE

const size_t OPID_BUF_SIZE = 64;

//
// This class provides bridge to PostgreSQL
//
class GOODS_DLL_EXPORT pgsql_storage : public dbs_storage { 
    work* txn;
    connection* con;
    opid_t opid_buf[OPID_BUF_SIZE];
    size_t opid_buf_pos;
    std::vector<class_descriptor*> descriptor_table;
    size_t max_preloaded_set_members;

  public:
    pgsql_storage(stid_t sid) : dbs_storage(sid, NULL), txn(NULL), con(NULL), opid_buf_pos(OPID_BUF_SIZE), max_preloaded_set_members(10) {}
	
    virtual objref_t allocate(cpid_t cpid, size_t size, int flags, objref_t clusterWith);
    virtual void    bulk_allocate(size_t sizeBuf[], cpid_t cpidBuf[], size_t nAllocObjects, 
                                  objref_t opidBuf[], size_t nReservedOids, hnd_t clusterWith[]);
    virtual void    deallocate(objref_t opid);

    virtual boolean lock(objref_t opid, lck_t lck, int attr);
    virtual void    unlock(objref_t opid, lck_t lck);
    
    //
    // Get class descriptor by class identifier.
    // Class descriptor is placed in the buffer supplied by application.
    // If there is no such class at server buf.size() is set to 0.
    //
    virtual void    get_class(cpid_t cpid, dnm_buffer& buf);
    //
    // Store class descriptor at server receive it's identifier
    //
    virtual cpid_t  put_class(dbs_class_descriptor* desc);
    //
    // Change existed class descriptor
    //
    virtual void    change_class(cpid_t cpid, dbs_class_descriptor* desc);
    //                                        
    // Send a message to others clients. 
    //
    virtual void    send_message( int message);
    //
    // Schedule a user message to others clients. 
    //
    virtual void    push_message( int message);
    //
    // Send all scheduleds user messages to others clients. 
    //
    virtual void    send_messages();
                                            
    //
    // Load object from server into the buffer supplied by application.
    // Before each object dbs_object_header structure is placed.
    // If there is no such object at server then "cpid" field of 
    // dbs_object_header is set to 0.
    //
    virtual void    load(objref_t* opid, int n_objects, 
                         int flags, dnm_buffer& buf);

    virtual void    load(objref_t opid, int flags, dnm_buffer& buf);


    virtual void    query(objref_t& next_mbr, char const* query, nat4 buf_size, int flags, nat4 max_members, dnm_buffer& buf);

    //
    // Inform server that client no more has reference to specified object
    // 
    virtual void    forget_object(objref_t opid);
    //
    // Inform server that client no more has instance of specified object
    //
    virtual void    throw_object(objref_t opid); 

    //
    // Initiate transaction at server. Allocate place for transaction header
    // in buffer. All objects involved in transaction should be appended 
    // to this buffer. 
    //
    virtual void    begin_transaction(dnm_buffer& buf); 
    
    //
    // Rollback transaction in which this storage was involved
    //
    virtual void    rollback_transaction();    

    //
    // Commit local transaction or part of global transaction 
    // at coordinator server. In latter case coordinator will return 
    // transaction indentifier which should be 
    // passed to all other servers participated in transaction.
    // If transaction is aborted by server "False" is returned. 
    //
    virtual boolean commit_coordinator_transaction(int n_trans_servers, 
                                                   stid_t* trans_servers,
                                                   dnm_buffer& buf, 
                                                   trid_t& tid);
    //
    // Commit local part of global transaction at server. 
    //
    virtual void    commit_transaction(stid_t coordinator, 
                                       int n_trans_servers,
                                       stid_t* trans_servers,
                                       dnm_buffer& buf, 
                                       trid_t tid);
    //
    // Wait completion of global transaction (request to coordinator)
    // If transaction is aborted by server "False" is returned. 
    //
    virtual boolean wait_global_transaction_completion();

    virtual boolean open(const char* server_connection_name, char const* login = NULL, char const* password = NULL);
    virtual void    close();

    virtual nat8    get_used_size();

    virtual boolean start_gc();

    virtual void    add_user(char const* login, char const* password);
    virtual void    del_user(char const* login);

    class_descriptor* lookup_class(cpid_t cpid);
    void unpack_object(std::string const& prefix, class_descriptor* desc, dnm_buffer& buf, result::tuple const& record);
    objref_t load_query_result(result& rs, dnm_buffer& buf);
    size_t store_struct(field_descriptor* first, invocation& stmt, char* &src_refs, char* &src_bins, size_t size);

    invocation statement(char const* name);

    ref<set_member> index_find(database const* db, objref_t index, char const* op, std::string const& key);
    void hash_put(objref_t hash, const char* name, objref_t opid);
    objref_t hash_get(objref_t hash, const char* name);
    bool hash_del(objref_t hash, const char* name);
    bool hash_del(objref_t hash, const char* name, objref_t opid);
    void hash_drop(objref_t hash);
    void hash_size(objref_t hash);
    void create_table(class_descriptor* desc);
    void start_transaction();
};


class GOODS_DLL_EXPORT pgsql_index : public B_tree
{
  public:
    virtual ref<set_member> find(const char* str, size_t len, skey_t key) const;
    virtual ref<set_member> find(const char* str) const;
    virtual ref<set_member> find(skey_t key) const;
    virtual ref<set_member> findGE(skey_t key) const;
    virtual ref<set_member> findGE(const char* str, size_t len, skey_t key) const;
    virtual ref<set_member> findGE(const char* str) const;

    virtual void insert(ref<set_member> mbr);
    virtual void remove(ref<set_member> mbr);
    virtual void clear();

    static ref<pgsql_index> create(anyref obj) {
        return NEW pgsql_index(obj);
    }

    METACLASS_DECLARATIONS(pgsql_index, B_tree);
    pgsql_index(anyref const& obj) : B_tree(self_class, obj, 0) {}
    pgsql_index(class_descriptor& desc, ref<object> const& obj) : B_tree(desc, obj, 0) {}
};

class GOODS_DLL_EXPORT pgsql_dictionary : public dictionary { 
  public: 
    //
    // Add to hash table association of object with specified name.
    //
    virtual void        put(const char* name, anyref obj);
    //
    // Search for object with specified name in hash table.
    // If object is not found NULL is returned.
    //
    virtual anyref      get(const char* name) const;
    //
    // Remove object with specified name from hash table.
    // If there are several objects with the same name the one last inserted
    // is removed. If such name was not found 'False' is returned.
    //
    virtual boolean     del(const char* name);
    //
    // Remove concrete object with specified name from hash table.
    // If such name was not found or it is associated with different object
    // 'False' is returned. If there are several objects with the same name, 
    // all of them are compared with specified object.
    //    
    virtual boolean     del(const char* name, anyref obj);

    virtual anyref apply(hash_item::item_function) const;	

    virtual void reset();

    virtual size_t get_number_of_elements() const;

    METACLASS_DECLARATIONS(pgsql_dictionary, dictionary);
    
    pgsql_dictionary() : dictionary(self_class) {}

    static ref<pgsql_dictionary> create(size_t estimation = 10001) {
        return NEW pgsql_dictionary();
    }
};    


#endif
