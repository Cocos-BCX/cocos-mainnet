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

class call_contract_function_evaluator : public evaluator<call_contract_function_evaluator>
{
  public:
    typedef call_contract_function_operation operation_type;
    void_result evaluate_contract_authority(contract_id_type contract_id,const flat_set<public_key_type> &sigkeys);
    contract_result apply(account_id_type caller, string function_name, vector<lua_types> value_list,
                          transaction_apply_mode run_mode, optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys);
    void_result do_evaluate(const operation_type &o);
    contract_result do_apply(const operation_type &o);
    void pay_fee_for_result(contract_result& result);
    const contract_object *contract_pir= nullptr;
    const contract_bin_code_object *contract_code_pir= nullptr;
    const operation_type* op;

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
