
#pragma once
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/lua_types.hpp>

namespace graphene { namespace chain {

   struct contract_create_operation : public base_operation
   {
      struct fee_parameters_type {
         uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; 
      };
      account_id_type  owner;      // 合约创建者
      string           name;       // 合约名字
      string           data;       // 合约内容
      public_key_type  contract_authority;//合约权限
      extensions_type  extensions;
      account_id_type fee_payer()const { return owner; }
      void            validate()const
      {
        FC_ASSERT(memcmp(name.data(),"contract.",9)==0);
        FC_ASSERT( is_valid_name( name ) ); //验证合约名字
                                            //后期考虑加入合约权限
      }
      share_type      calculate_fee(const fee_parameters_type& schedule)const
      {
        share_type core_fee_required = schedule.fee;
        core_fee_required += calculate_data_fee( fc::raw::pack_size(data), schedule.price_per_kbyte );
        return core_fee_required;
      };
   };

   struct revise_contract_operation : public base_operation
   {
     struct fee_parameters_type {
         uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION; 
      };
      account_id_type  reviser;     // 合约创建者
      contract_id_type contract_id; // 合约名字
      string           data;        // 合约内容
      extensions_type  extensions;
      account_id_type fee_payer()const { return reviser; }
      void            validate()const
      {
      }
      share_type      calculate_fee(const fee_parameters_type& schedule)const
      {
        share_type core_fee_required = schedule.fee;
        core_fee_required += calculate_data_fee( fc::raw::pack_size(data), schedule.price_per_kbyte );
        return core_fee_required;
      };
   };

   struct call_contract_function_operation : public base_operation
   {
     struct fee_parameters_type {
         uint64_t fee       = 20 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_kbyte = 10 * GRAPHENE_BLOCKCHAIN_PRECISION;
         uint32_t price_per_millisecond=10 * GRAPHENE_BLOCKCHAIN_PRECISION;
      };
      account_id_type       caller;       // 合约调用者
      contract_id_type      contract_id;  // 合约ID
      string                function_name;// 目标函数名
      vector<lua_types>     value_list;   // 参数列表
      extensions_type       extensions;   
      account_id_type       fee_payer()const { return caller; }
      void                  validate()const
      {
                                            //后期考虑加入合约权限
      }
      uint64_t calculate_run_time_fee( uint64_t run_time_by_us, uint64_t price_per_millisecond )const
      {
        auto result = (fc::uint128(run_time_by_us) * price_per_millisecond) / 1000;
        FC_ASSERT( result <= GRAPHENE_MAX_SHARE_SUPPLY );
        return result.to_uint64();
      }
      share_type      calculate_fee(const fee_parameters_type& schedule)const
      {
        share_type core_fee_required = schedule.fee;
        core_fee_required += calculate_data_fee( fc::raw::pack_size(function_name)+
                                                 fc::raw::pack_size(value_list)+
                                                 fc::raw::pack_size(extensions)
                                                 , schedule.price_per_kbyte );
        //core_fee_required+=calculate_run_time_fee(run_time.value, schedule.price_per_millisecond);
        return core_fee_required;
      };
          

   };
}} // graphene::chain

FC_REFLECT( graphene::chain::contract_create_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::contract_create_operation, (owner)(name)(data)(contract_authority)(extensions) )

FC_REFLECT( graphene::chain::revise_contract_operation::fee_parameters_type, (fee)(price_per_kbyte) )
FC_REFLECT( graphene::chain::revise_contract_operation, (reviser)(contract_id)(data)(extensions) )

FC_REFLECT( graphene::chain::call_contract_function_operation::fee_parameters_type, (fee)(price_per_kbyte)(price_per_millisecond) )
FC_REFLECT( graphene::chain::call_contract_function_operation, (caller)(contract_id)(function_name)(value_list)(extensions) )
