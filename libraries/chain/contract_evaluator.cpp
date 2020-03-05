#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene
{

namespace chain
{
uint64_t contract_private_data_size = 3 * 1024;
uint64_t contract_total_data_size = 10 * 1024 * 1024;
uint64_t contract_max_data_size = 2 * 1024 * 1024 * 1024;

void_result contract_create_evaluator::do_evaluate(const operation_type &o)
{

    return void_result(); //TODO: add verification in future
}
object_id_result contract_create_evaluator::do_apply(const operation_type &o)
{
    try
    {
        database &d = db();
        contract_object co(o.name);
        auto next_id = d.get_index_type<contract_index>().get_next_id();
        co.id=next_id;
        lua_table aco = co.do_contract(o.data,d.get_luaVM().mState);
        auto code_bin_object= d.create<contract_bin_code_object>([&](contract_bin_code_object&cbo){cbo.contract_id=next_id;co.get_code(cbo.lua_code_b);});
        contract_object contract = d.create<contract_object>([&](contract_object &c) {
            c.owner = o.owner;
            c.name = o.name;

            if (next_id != contract_id_type())
            {
                c.contract_authority = o.contract_authority;
                c.current_version = trx_state->_trx->hash();
            }
            c.lua_code_b_id = code_bin_object.id;
            c.creation_date = d.head_block_time();
            c.contract_ABI = aco.v;
        });
        return contract.id;
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result contract_share_evaluator::do_evaluate(const operation_type &o)
{
    return void_result(); //TODO: add verification in future
}
void_result contract_share_evaluator::do_apply(const operation_type &o)
{
   return void_result(); 
}

void_result contract_share_fee_evaluator::do_evaluate(const operation_type &o)
{
    return void_result(); //TODO: add verification in future
}
void_result contract_share_fee_evaluator::do_apply(const operation_type &o)
{
   return void_result(); 
}


void_result revise_contract_evaluator::do_evaluate(const operation_type &o)
{
    try
    {
        database &d = db();
        auto &contract = o.contract_id(d);
        auto &contract_owner = contract.owner(d);
        FC_ASSERT(!contract.is_release," The current contract is  release version cannot be change ");
        FC_ASSERT(contract_owner.asset_locked.contract_lock_details.find(o.contract_id) == contract_owner.asset_locked.contract_lock_details.end());
        FC_ASSERT(contract_owner.get_id() == o.reviser, "You do not have the authority to modify the contract,the contract owner is ${owner}",
                  ("owner", contract_owner.get_id()));
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

logger_result revise_contract_evaluator::do_apply(const operation_type &o)
{
    try
    {
        database &_db = db();
        auto &old_contract = o.contract_id(_db);
        contract_object co = old_contract;
        lua_table aco = co.do_contract(o.data, _db.get_luaVM().mState);
        auto &_old_code_bin_object=old_contract.lua_code_b_id(_db);
        _db.modify(_old_code_bin_object,[&](contract_bin_code_object&cbo){co.get_code(cbo.lua_code_b);});
        string previous_version;
        _db.modify(old_contract, [&](contract_object &c) {
            previous_version = fc::string(c.current_version);
            c.current_version = trx_state->_trx->hash();
            c.contract_ABI = aco.v;
        });
        return logger_result(previous_version);
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result call_contract_function_evaluator::do_evaluate(const operation_type &o)
{
    try
    {
        this->op = &o;
        FC_ASSERT(o.contract_id!=contract_id_type());
        evaluate_contract_authority(o.contract_id, trx_state->sigkeys);
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}
void_result call_contract_function_evaluator::evaluate_contract_authority(contract_id_type contract_id, const flat_set<public_key_type> &sigkeys)
{
    database &d = db();
    contract_pir = &contract_id(d);
    contract_code_pir=&(contract_pir->lua_code_b_id(d));
    if (contract_pir->check_contract_authority)
    {
        wlog("check_contract_authority_falg");
        if (!(trx_state->skip & (database::validation_steps::skip_transaction_signatures |database::validation_steps::skip_authority_check)))
        {
            auto key_itr = std::find(sigkeys.begin(), sigkeys.end(), contract_pir->contract_authority);
            FC_ASSERT(key_itr != sigkeys.end(), "No contract related permissions were found in the signature,contract_authority:${contract_authority}",
                  ("contract_authority", contract_pir->contract_authority));
        }
    }
    return void_result();
}

contract_result call_contract_function_evaluator::do_apply(const operation_type &o)
{
    try
    {
        database &d = db();
        optional<contract_result> _contract_result;
        if (trx_state->run_mode == transaction_apply_mode::apply_block_mode)
        {
            processed_transaction *ptx = (processed_transaction *)trx_state->_trx;
            vector<operation_result> operation_results = ptx->operation_results;
            _contract_result = operation_results[d.get_current_op_index()].get<contract_result>();
        }
        else
            _contract_result = contract_result();
        return apply(o.caller, o.function_name, o.value_list, _contract_result, trx_state->sigkeys);
    }
    FC_CAPTURE_AND_RETHROW((o))
}
void call_contract_function_evaluator::pay_fee_for_result(contract_result &result)
{
    auto &fee_schedule_ob = db().current_fee_schedule();
    auto &op_fee = fee_schedule_ob.get<operation_type>();
    share_type temp = op->calculate_data_fee(result.relevant_datasize, op_fee.price_per_kbyte);
    temp += op->calculate_run_time_fee(*result.real_running_time, op_fee.price_per_millisecond);
    auto additional_cost = fc::uint128(temp.value) * fee_schedule_ob.scale / GRAPHENE_100_PERCENT;
    core_fee_paid += share_type(fc::to_int64(additional_cost));

    contract_id_type db_index = result.contract_id;
    database &_db = db();
    const contract_object &contract_obj = db_index(_db); 
    result.sharer = contract_obj.owner; 
    result.total_fees.amount = core_fee_paid;

    auto invoke_percent = 0;
    if(contract_obj.user_invoke_share_percent>100)
      invoke_percent = 100;
    else if(contract_obj.user_invoke_share_percent<0)
      invoke_percent = 0;
    else
      invoke_percent = contract_obj.user_invoke_share_percent;
      
    auto user_invoke_share_fee =  core_fee_paid*invoke_percent/100;
    user_invoke_creator_fee = core_fee_paid - user_invoke_share_fee;
    core_fee_paid = user_invoke_share_fee;
    ilog("--user_invoke_share_fee ${x}", ("x", user_invoke_share_fee.value));
    ilog("--user_invoke_creator_fee ${x}", ("x", user_invoke_creator_fee.value));
    if((user_invoke_share_fee.value<0)||(user_invoke_creator_fee.value<0))
    ilog("--user_invoke_share_fee or user_invoke_creator_fee");
}

contract_result call_contract_function_evaluator::do_apply_function(account_id_type caller, string function_name,vector<lua_types> value_list,
                                                                    optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys,contract_id_type  contract_id)
{
    try
    {
        //fc::microseconds start = fc::time_point::now().time_since_epoch();
        database &_db = db();
        //auto &contract_core_index = _db.get_index_type<contract_index>().indices().get<by_id>();
        //auto contract_itr = contract_core_index.find(contract_id);
        //FC_ASSERT(contract_itr != contract_core_index.end(), "The specified contract does not exist.contract_id:${contract_id}", ("contract_id", contract_id));
        contract_object contract = *contract_pir;
        contract.set_code(contract_code_pir->lua_code_b);        
        contract.set_mode(trx_state);
        contract.set_process_encryption_helper(process_encryption_helper(_db.get_chain_id().str(), string(CONTRACT_PROCESS_CIPHER), _db.head_block_time()));
        if (trx_state->run_mode == transaction_apply_mode::apply_block_mode && _contract_result->existed_pv)
        {
            contract.set_process_value(_contract_result->process_value);
        }
        auto &contract_udata_index = _db.get_index_type<account_contract_data_index>().indices().get<by_account_contract>();
        auto old_account_contract_data_itr = contract_udata_index.find(boost::make_tuple(caller, contract_pir->id));
        optional<account_contract_data> op_acd;
        if (old_account_contract_data_itr != contract_udata_index.end())
            op_acd = *old_account_contract_data_itr;
        else
            op_acd = account_contract_data();   

        contract.do_contract_function(caller, function_name, value_list, op_acd->contract_data, _db, sigkeys, *_contract_result,contract_id);

        if (_options != nullptr)
        {
            if (_options->count("contract_private_data_size"))
            {
                auto tmp = _options->at("contract_private_data_size").as<uint64_t>();
                if ( tmp < contract_max_data_size && tmp >= 0 ) 
                {
                    contract_private_data_size = tmp;
                }
            }

            if (_options->count("contract_total_data_size"))

            {
                auto tmp = _options->at("contract_total_data_size").as<uint64_t>();
                if ( tmp < contract_max_data_size && tmp >= 0 ) 
                {
                    contract_total_data_size = tmp;
                }
            }

        }
        FC_ASSERT(fc::raw::pack_size(op_acd->contract_data) <= contract_private_data_size, "call_contract_function_evaluator::apply, the contract private data size is too large.");
        FC_ASSERT(fc::raw::pack_size(contract.contract_data) <= contract_total_data_size, "call_contract_function_evaluator::apply, the contract total data size is too large.");

        // wdump(("do_contract_function")(fc::time_point::now().time_since_epoch() - start));
        //start = fc::time_point::now().time_since_epoch();
        if (old_account_contract_data_itr == contract_udata_index.end())
            _db.create<account_contract_data>([&](account_contract_data &a) {
                a.owner = caller;
                a.contract_id = contract_pir->id;
                a.contract_data = op_acd->contract_data;
            });
        else
           _db.modify(*old_account_contract_data_itr, [&](account_contract_data &a) {
                a.contract_data = op_acd->contract_data;
            });
        _db.modify(*contract_pir, [&](contract_object &co) {
            co.contract_data = contract.contract_data;
        });
        //wdump(("write data")(fc::time_point::now().time_since_epoch() - start));
        return contract.get_result();
    }
    FC_CAPTURE_AND_RETHROW()
}


contract_result call_contract_function_evaluator::apply(account_id_type caller, string function_name,
                                                        vector<lua_types> value_list, optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys)
{
    contract_id_type  contract_id;
    return do_apply_function(caller,function_name,value_list,_contract_result,sigkeys,contract_id);
}

contract_result call_contract_function_evaluator::apply(account_id_type caller, contract_id_type  contract_id,string function_name,
                                                        vector<lua_types> value_list, optional<contract_result> &_contract_result, const flat_set<public_key_type> &sigkeys)
{
    return do_apply_function(caller,function_name,value_list,_contract_result,sigkeys,contract_id);
}


} // namespace chain
} // namespace graphene
