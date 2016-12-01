//-< GUESS.CXX >-----------------------------------------------------*--------*
// GOODS                     Version 1.0         (c) 1997  GARRET    *     ?  *
// (Generic Object Oriented Database System)                         *   /\|  *
//                                                                   *  /  \  *
//                          Created:      7-Jun-97    K.A. Knizhnik  * / [] \ *
//                          Last update: 17-Oct-97    K.A. Knizhnik  * GARRET *
//-------------------------------------------------------------------*--------*
// Example program: game "Guess an animal"
//-------------------------------------------------------------------*--------*

#include "goods.h"
#include "pgsql_storage.h"

USE_GOODS_NAMESPACE

#define MAX_STRING_SIZE 256

class Guess : public object {
  public: 
    ref<Guess>    yes;
    ref<Guess>    no;
    char          question[1]; 

    void          guess() const;

    ref<Guess>    ask() const; 

    static Guess* who(ref<Guess> const& parent); 

    static Guess* create(ref<Guess> const& no, char* question, 
                         ref<Guess> const& yes);


    METACLASS_DECLARATIONS(Guess, object);

  protected: 
    Guess(ref<Guess> const& no, char* question, ref<Guess> const& yes); 
};


Guess::Guess(ref<Guess> const& no, char* question, ref<Guess> const& yes)
: object(self_class, strlen(question)+1)
{
    this->no  = no;
    this->yes = yes;
    strcpy(this->question, question);
}

Guess* Guess::create(ref<Guess> const& no, char* question, 
                     ref<Guess> const& yes)
{
    return new (self_class, strlen(question)+1) Guess(no, question, yes);
}    

void input(char* prompt, char* buf, size_t buf_size)
{
    char* p;
    do { 
        console::output(prompt);
        *buf = '\0';
        console::input(buf, buf_size);
        p = buf + strlen(buf);
    } while (p <= buf+1); 
    
    if (*(p-1) == '\n') {
        *--p = '\0';
    }
}

boolean positive_answer()
{
    char buf[16];
    return console::input(buf, sizeof buf) && (*buf == 'y' || *buf == 'Y');
}


Guess* Guess::who(ref<Guess> const& parent) 
{
    char animal[MAX_STRING_SIZE], question[MAX_STRING_SIZE];
    input("What is it ?\n", animal, sizeof animal);
    input("What is difference from other ?\n", question, sizeof question);
    return create(parent, question, create(NULL, animal, NULL));
}

ref<Guess> Guess::ask() const
{  
    console::output("May be, %s (y/n) ? ", question);
    if (positive_answer()) { 
        if (yes.is_nil()) { 
            console::output("It was very simple question for me...\n");
        } else { 
            ref<Guess> clarify = yes->ask();
            if (!clarify.is_nil()) { 
                modify(this)->yes = clarify;
            }
        }
    } else { 
        if (no.is_nil()) { 
            if (yes.is_nil()) { 
                return who(this);
            } else {
                modify(this)->no = who(NULL);
            } 
        } else { 
            ref<Guess> clarify = no->ask();
            if (!clarify.is_nil()) { 
                modify(this)->no = clarify;
            }
        }
    }
    return NULL; 
}

void Guess::guess() const
{
    if (is_abstract_root()) { 
        modify(this)->become(who(NULL));
    } else { 
        ask();
    }
}

field_descriptor& Guess::describe_components()
{
    return FIELD(yes), FIELD(no), VARYING(question);
}

void on_transaction_abort(metaobject*)
{
    console::output("Let's try again...\n");
}
    
REGISTER(Guess, object, optimistic_repeatable_read_scheme);

class pgsql_database : public database { 
  public:
    virtual dbs_storage* create_dbs_storage(stid_t sid) const {
		return new pgsql_storage(sid);
	}
};	

int main(int argc, char const* argv[]) 
{
    ref<Guess> root;
	pgsql_database db;

    if (db.open("pgsql.cfg", "", "")) { 
        storage_root::self_class.mop = &optimistic_repeatable_read_scheme; 
        optimistic_repeatable_read_scheme.set_abort_transaction_hook(on_transaction_abort);
		
		if (argc > 1 && strcmp(argv[1], "convert") == 0) { 
			if (db.convert_goods_database(".", "guess")) { 
				printf("GOODS database successully converted\n");
			} else {
				fprintf(stderr, "Failed to convert GOODS database\n");
			} 
	    }
		
        db.get_root(root);

        while(True) { 
            console::output("Think of an animal.\nReady (y/n) ? ");
            if (!positive_answer())
            {
                break;
            }
            root->guess();
        }
        console::output("End of the game\n");
        db.close();
        return EXIT_SUCCESS;
    } else { 
        console::output("Failed to open database\n");
        return EXIT_FAILURE;
    }
}



