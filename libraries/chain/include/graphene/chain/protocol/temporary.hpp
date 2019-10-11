#pragma once
#include <graphene/chain/protocol/base.hpp>
namespace graphene { namespace chain {
 struct temporary_authority_change_operation : public base_operation
   {
      struct fee_parameters_type {
         uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; 
      };
      account_id_type owner;       // 拥有者
      string          describe;    // 描述
      flat_map<public_key_type,weight_type> temporary_active; // 权限公钥, 以及对应权重
      fc::time_point_sec expiration_time;
      extensions_type  extensions;
      account_id_type fee_payer()const { return owner; }
      void            validate()const
      {
        FC_ASSERT( temporary_active.size()!=0);
      }
      share_type      calculate_fee(const fee_parameters_type& schedule)const
      {
        share_type core_fee_required = schedule.fee;
        core_fee_required += calculate_data_fee( fc::raw::pack_size(describe), schedule.price_per_kbyte );
        return core_fee_required;
      };
   };
}}
FC_REFLECT( graphene::chain::temporary_authority_change_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::temporary_authority_change_operation, (owner)(describe)(temporary_active)(expiration_time)(extensions) )