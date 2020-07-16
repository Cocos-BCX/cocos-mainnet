#pragma once
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/contract_object.hpp>
namespace graphene
{
namespace chain
{

class contract_create_evaluator : public evaluator<contract_create_evaluator>
{
  public:
    typedef contract_create_operation operation_type;
    void_result do_evaluate(const operation_type &o);
    object_id_result do_apply(const operation_type &o);
};

class contract_share_fee_evaluator : public evaluator<contract_share_fee_evaluator>
{
  public:
    typedef contract_share_fee_operation operation_type;
    void_result do_evaluate(const operation_type &o);
    void_result do_apply(const operation_type &o);
};

class call_contract_function_evaluator : public evaluator<call_contract_function_evaluator>
{
  public:
    typedef call_contract_function_operation operation_type;
    void_result evaluate_contract_authority(contract_id_type contract_id,const flat_set<public_key_type> &sigkeys);
    contract_result apply(account_id_type caller,string function_name, vector<lua_types> value_list,
                          optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys);
    contract_result apply(account_id_type caller, contract_id_type  contract_id,string function_name, vector<lua_types> value_list,
                          optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys);
    contract_result do_apply_function(account_id_type caller,string function_name, vector<lua_types> value_list,
                          optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys,contract_id_type  contract_id);
    void_result do_evaluate(const operation_type &o);
    contract_result do_apply(const operation_type &o);
    void pay_fee_for_result(contract_result& result);
    virtual void pay_fee() override;
    const contract_object *contract_pir= nullptr;
    const contract_bin_code_object *contract_code_pir= nullptr;
    const operation_type* op;

private:
    void pay_fee_impl() ;
    void account_pay_fee(const account_id_type &account_id, const share_type& account_core_fee_paid, uint32_t percent);
    
  private:
    share_type user_invoke_creator_fee;

};

class revise_contract_evaluator : public evaluator<revise_contract_evaluator>
{
  public:
    typedef revise_contract_operation operation_type;
    void_result do_evaluate(const operation_type &o);
    logger_result do_apply(const operation_type &o);
};

} // namespace chain
} // namespace graphene
