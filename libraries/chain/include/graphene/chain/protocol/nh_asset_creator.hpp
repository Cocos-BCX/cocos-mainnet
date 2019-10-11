
#pragma once
#include <graphene/chain/protocol/base.hpp>


namespace graphene { namespace chain { 
   //注册游戏开发者
   struct register_nh_asset_creator_operation : public base_operation
   {
       struct fee_parameters_type { uint64_t fee =  GRAPHENE_BLOCKCHAIN_PRECISION; };
       account_id_type    fee_paying_account;  //游戏开发者注册人
       account_id_type fee_payer()const { return fee_paying_account; }
       void            validate() const;
   };


   
}} // graphene::chain

FC_REFLECT( graphene::chain::register_nh_asset_creator_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::register_nh_asset_creator_operation,(fee_paying_account) )



