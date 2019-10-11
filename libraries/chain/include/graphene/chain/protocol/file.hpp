// created by zhangfan 
// the operation about file

#pragma once
#include <graphene/chain/protocol/base.hpp>


namespace graphene { namespace chain { 

   //create file
   struct create_file_operation : public base_operation
   {
       struct fee_parameters_type { 
	   	  uint64_t fee =  GRAPHENE_BLOCKCHAIN_PRECISION; 
		  uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION;
	   }; 
       account_id_type    file_owner;  // the file creator
       string file_name;  // file name
       string file_content;  // file content
       account_id_type fee_payer()const { return file_owner; }
       void            validate() const;
	   share_type      calculate_fee(const fee_parameters_type& k)const;
   };

   // add accounts related to a file
   struct add_file_relate_account_operation : public base_operation
   {
       struct fee_parameters_type { 
	   	  uint64_t fee =  GRAPHENE_BLOCKCHAIN_PRECISION; 
		  uint32_t price_per_related_account = GRAPHENE_BLOCKCHAIN_PRECISION;
	   }; 
       account_id_type file_owner;  // the file owner
       file_id_type    file_id;  // file id
       flat_set<account_id_type> related_account; // the accounts who will be related to the file
       account_id_type fee_payer()const { return file_owner; }
       void            validate() const;
	   share_type      calculate_fee(const fee_parameters_type& k)const;
   };

   // add signature into a file
   struct file_signature_operation : public base_operation
   {
       struct fee_parameters_type { 
	   	  uint64_t fee =  GRAPHENE_BLOCKCHAIN_PRECISION; 
		  uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION;  // only required for large signature.
	   };
       account_id_type signature_account;  // the account who signed
       file_id_type    file_id;  // file id
       string signature; // signature content
       account_id_type fee_payer()const { return signature_account; }
       void            validate() const;
	   share_type      calculate_fee(const fee_parameters_type& k)const;
   };

   // related a sub file to a parent file
   struct relate_parent_file_operation : public base_operation
   {
       struct fee_parameters_type { uint64_t fee =  GRAPHENE_BLOCKCHAIN_PRECISION; };  
       account_id_type    sub_file_owner;  // the sub file's owner
       file_id_type parent_file;  // the parent file's id
       //flat_set<account_id_type> parent_file_related_account; // the related accounts of parent file
       account_id_type    parent_file_owner;  // the parent file's owner
       file_id_type sub_file;  // the sub file's owner
       account_id_type fee_payer()const { return parent_file_owner; }
       void            validate() const;
   };
}} // graphene::chain

FC_REFLECT(graphene::chain::create_file_operation::fee_parameters_type, (fee)(price_per_kbyte))

FC_REFLECT(graphene::chain::create_file_operation, (file_owner)(file_name)(file_content))

FC_REFLECT(graphene::chain::add_file_relate_account_operation::fee_parameters_type, (price_per_related_account))

FC_REFLECT(graphene::chain::add_file_relate_account_operation, (file_owner)(file_id)(related_account))

FC_REFLECT(graphene::chain::file_signature_operation::fee_parameters_type, (fee)(price_per_kbyte))

FC_REFLECT(graphene::chain::file_signature_operation, (signature_account)(file_id)(signature))

FC_REFLECT(graphene::chain::relate_parent_file_operation::fee_parameters_type, (fee))

FC_REFLECT( graphene::chain::relate_parent_file_operation,(sub_file_owner)(parent_file)(parent_file_owner)(sub_file) )
