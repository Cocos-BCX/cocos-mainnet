/*
   creat by zhangfan
   declaration of the file object
   define the multi index of the file object   
 */

#pragma once

#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {


	class file_object : public abstract_object<file_object>
	{
	   
	   public:
		  static const uint8_t space_id = protocol_ids;
		  static const uint8_t type_id	= file_object_type;

		  string  file_name;
		  account_id_type file_owner;
		  time_point_sec create_time;
		  string file_content;
		  flat_set<account_id_type> related_account;
		  map<account_id_type, string> signature;
		  optional<file_id_type> parent_file;
		  vector<file_id_type> sub_file;
	};

	
	struct by_file_name{};
	struct by_file_owner_and_name{};
	struct by_file_create_time{};
	
	typedef multi_index_container<
	   file_object,
	   indexed_by<
		  ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
		  ordered_unique< tag<by_file_name>, member< file_object, string, &file_object::file_name > >,
		  ordered_unique< tag<by_file_owner_and_name>,
	         composite_key< file_object,
	            member< file_object, account_id_type, &file_object::file_owner>,
	            member< file_object, string, &file_object::file_name >
	         >
	      >,
	      ordered_non_unique< tag<by_file_create_time>, member< file_object, time_point_sec, &file_object::create_time > >
    >
	> file_object_multi_index_type;
	
	typedef generic_index<file_object, file_object_multi_index_type> file_index;


} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::file_object,
                    (graphene::db::object),
                    (file_name)(file_owner)(create_time)(file_content)(related_account)(signature)(parent_file)(sub_file) )

