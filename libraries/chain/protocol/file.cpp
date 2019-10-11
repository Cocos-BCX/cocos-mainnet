// creat by zhangfan 
#include <graphene/chain/protocol/operations.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>

#include <graphene/chain/protocol/file.hpp>

namespace graphene { namespace chain {

void create_file_operation::validate() const 
{
   FC_ASSERT( !file_name.empty(), "The file name is empty" );
   FC_ASSERT( 3 <= file_name.size() && 20 >= file_name.size(), "The file name'length must between 3 and 20" );
   FC_ASSERT( !std::isdigit(file_name[0]), "The file name cat't start with a digit" );
   FC_ASSERT( !file_content.empty(), "The file content is empty" );
   return;
}

share_type create_file_operation::calculate_fee( const fee_parameters_type& schedule )const
{
   share_type core_fee_required = schedule.fee;
   core_fee_required += calculate_data_fee( fc::raw::pack_size(file_content), schedule.price_per_kbyte );
   return core_fee_required;
}


void add_file_relate_account_operation::validate() const 
{
   FC_ASSERT( !related_account.empty(), "The file relate account is empty" );
   return;
}

share_type add_file_relate_account_operation::calculate_fee( const fee_parameters_type& schedule )const
{
   share_type core_fee_required = schedule.fee;
   core_fee_required += related_account.size() * schedule.price_per_related_account;
   return core_fee_required;
}


void file_signature_operation::validate() const 
{
   FC_ASSERT( !signature.empty(), "The signature is empty" );
   return;
}

share_type file_signature_operation::calculate_fee( const fee_parameters_type& schedule )const
{
   share_type core_fee_required = schedule.fee;
   core_fee_required += calculate_data_fee( fc::raw::pack_size(signature), schedule.price_per_kbyte );
   return core_fee_required;
}


void relate_parent_file_operation::validate() const 
{
   //Verify that the parent file and the sub file are the same file
   FC_ASSERT( parent_file != sub_file , "The sub file and parent file can't be one file." );
   return;
}

} } // graphene::chain
