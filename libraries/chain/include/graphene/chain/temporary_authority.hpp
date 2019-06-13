#pragma once
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/generic_index.hpp>
#include <boost/multi_index/composite_key.hpp>
namespace graphene { namespace chain {
    
class temporary_active_object : public graphene::db::abstract_object<temporary_active_object>
   {
      public:
        static const uint8_t space_id = extension_id_for_nico;
        static const uint8_t type_id  = temporary_authority;
        account_id_type owner;
        flat_map<public_key_type,weight_type> temporary_active;
        string describe; 
        time_point_sec expiration_time;
        temporary_active_object_id_tyep get_id(){return id;}
    };
/*********************************************合约索引表***********************************************/
struct by_account_id_type{};
struct by_account_and_describe{};
struct by_id;
struct by_expiration;
   typedef multi_index_container<
      temporary_active_object,
      indexed_by<
         ordered_unique< 
                        tag<by_id>, 
                        member< object, object_id_type, &object::id > 
                        >,
         ordered_unique< 
                        tag<by_account_id_type>, 
                        member<temporary_active_object, account_id_type, &temporary_active_object::owner > 
                        >,
        ordered_unique
            < 
                tag<by_account_and_describe>,
                composite_key
                <
                        temporary_active_object ,
                        member<temporary_active_object , account_id_type, &temporary_active_object::owner>,
                        member<temporary_active_object , std::string, &temporary_active_object::describe>
                >
            >,
         ordered_non_unique< 
                tag<by_expiration>,
                member<temporary_active_object, time_point_sec, &temporary_active_object::expiration_time  >
            >                    
      >
   > temporary_active_multi_index_type;

   /**
    * @ingroup object_index
    */
   typedef generic_index<temporary_active_object, temporary_active_multi_index_type> temporary_active_index;

} }

FC_REFLECT_DERIVED(graphene::chain::temporary_active_object,
                    (graphene::db::object),
                    (owner)(temporary_active)(describe)(expiration_time)
                    )