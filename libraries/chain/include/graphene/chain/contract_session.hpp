#include<fc/include/fc/time.hpp>
#include<graphene/chain/protocol/types.hpp>
namespace graphene {
    namespace chain { 
        class contract_session
        {
            contract_id_type contract_id;
            fc::time_point expiry_time;//lastcalltime;
            typedef struct 
            {
                account_id_type user_id;
                fc::time_point expiry_time;
            }session_user;
            typedef struct 
            {
                fc::time_point expiry_time;
                uint64_t session_id; 
                vector<session_user> session_users;  
            }session;
            vector<session> sessions;

        };

    }}