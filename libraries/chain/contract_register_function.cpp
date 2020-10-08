﻿#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/contract_function_register_scheduler.hpp>
#include <graphene/chain/market_object.hpp>
namespace graphene
{
namespace chain
{

int register_scheduler::contract_random()
{
    try
    {
        this->result.existed_pv = true;
        srand(fc::time_point::now().time_since_epoch().count());
        auto temp = rand();
        int index = _process_value.next_random_index();
        if (trx_state->run_mode == transaction_apply_mode::apply_block_mode)
        {
            return _process_value.random[index];
        }
        else
        {
            _process_value.random.push_back(temp);
            return temp;
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
        return 0;
    }
}

uint64_t register_scheduler::real_time()
{
    try
    {
        this->result.existed_pv = true;
        auto temp = fc::time_point::now().time_since_epoch().count();
        int index = _process_value.next_time_index();
        if (trx_state->run_mode == transaction_apply_mode::apply_block_mode)
        {
            return _process_value.timetable[index];
        }
        else
        {
            _process_value.timetable.push_back(temp);
            return temp;
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
        return 0;
    }
}

memo_data register_scheduler::make_memo(string receiver_id_or_name, string key, string value, uint64_t ss, bool enable_logger)
{
    auto &receiver = get_account(receiver_id_or_name);
    try
    {
        memo_data md = memo_data();
        fc::ecc::private_key rand_key = fc::ecc::private_key::regenerate(fc::sha256::hash(key + std::to_string(ss))); //Get_random_private_key();
        md.from = rand_key.get_public_key();
        md.to = receiver.options.memo_key;
        md.set_message(rand_key, md.to, value, ss);
        if (enable_logger)
        {
            contract_memo_message cms;
            cms.affected_account = receiver.id;
            cms.memo = md;
            result.contract_affecteds.push_back(std::move(cms));
        }
        return md;
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
        return memo_data();
    }
}
void register_scheduler::invoke_contract_function(string contract_id_or_name, string function_name, string value_list_json)
{
    auto contract_id = get_contract(contract_id_or_name).id;
    try
    {
        FC_ASSERT(contract_id != this->contract.id, " You can't use it to make recursive calls. ");
        auto value_list = fc::json::from_string(value_list_json).as<vector<lua_types>>();
        call_contract_function_evaluator evaluator;
        auto state = *trx_state;
        state.sigkeys = sigkeys;
        evaluator.trx_state = &state;
        evaluator.evaluate_contract_authority(contract_id, state.sigkeys);
        //optional<contract_result> _contract_result;
        contract_result _contract_result;
        if (trx_state->run_mode == transaction_apply_mode::apply_block_mode)
        {
            for (auto contract_afd = apply_result.contract_affecteds.begin(); contract_afd != apply_result.contract_affecteds.end(); contract_afd++)
            {
                if (contract_afd->which() == contract_affected_type::tag<contract_result>::value)
                {
                    _contract_result = contract_afd->get<contract_result>(); //取出最顶上的委托值                    
                    apply_result.contract_affecteds.erase(contract_afd);     //移除已使用的委托值                    
                    break;
                }
            }
        }
        else
        {
            _contract_result = contract_result();
        }
        auto current_contract_name = context.readVariable<string>("current_contract");
        auto ocr = optional<contract_result>(_contract_result);
        auto ret = evaluator.apply(caller, this->contract.id,function_name, value_list, ocr, sigkeys);
        context.writeVariable("current_contract", current_contract_name);
        if (ret.existed_pv)
            result.existed_pv = true;
        result.contract_affecteds.push_back(ret);
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
void register_scheduler::log(string message)
{
    try
    {
        contract_logger logger(caller);
        logger.message = message;
        result.contract_affecteds.push_back(std::move(logger));
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
lua_Number register_scheduler::nummax()
{
    return std::numeric_limits<lua_Number>::max();
}
lua_Number register_scheduler::nummin()
{
    return std::numeric_limits<lua_Number>::min();
}
int64_t register_scheduler::integermax()
{
    return LUA_MAXINTEGER;
}
int64_t register_scheduler::integermin()
{
    return LUA_MININTEGER;
}
uint32_t register_scheduler::head_block_time()
{
    return db.head_block_time().sec_since_epoch();
}
string register_scheduler::hash256(string source)
{
    return fc::sha256::hash(source).str();
}
string register_scheduler::hash512(string source)
{
    return fc::sha512::hash(source).str();
}
const contract_object &register_scheduler::get_contract(string name_or_id)
{
    try
    {
        if (auto id = db.maybe_id<contract_id_type>(name_or_id))
        {
            const auto &contract_by_id = db.get_index_type<contract_index>().indices().get<by_id>();
            auto itr = contract_by_id.find(*id);
            FC_ASSERT(itr != contract_by_id.end(), "not find contract: ${contract}", ("contract", name_or_id));
            return *itr;
        }
        else
        {
            const auto &contract_by_name = db.get_index_type<contract_index>().indices().get<by_name>();
            auto itr = contract_by_name.find(name_or_id);
            FC_ASSERT(itr != contract_by_name.end(), "not find contract: ${contract}", ("contract", name_or_id));
            return *itr;
        }
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}
void register_scheduler::make_release()
{
    db.modify(contract.get_id()(db),[](contract_object& cb){cb.is_release=true;});
}


void register_scheduler::update_collateral_for_gas(string to, int64_t amount)
{
    try
    {
        FC_ASSERT(amount >= 0);
        share_type collateral = amount;
        account_id_type mortgager = caller;
        account_id_type beneficiary = get_account(to).id;
        const collateral_for_gas_object *collateral_for_gas = nullptr;
        auto &index = db.get_index_type<collateral_for_gas_index>().indices().get<by_mortgager_and_beneficiary>();
        auto itr = index.find(boost::make_tuple(mortgager, beneficiary));
        if (itr != index.end()) {
            collateral_for_gas = &(*itr);
        }

        share_type temp_supply = 0;
        if (collateral_for_gas)
        {
            auto temp_collateral = collateral - collateral_for_gas->collateral;
            auto temp_debt = db.estimation_gas(collateral).amount - collateral_for_gas->debt;
            db.modify(*collateral_for_gas, [&](collateral_for_gas_object &cgb) {
                cgb.collateral += temp_collateral;
                cgb.debt += temp_debt;
            });
            const auto &mortgager_obj = mortgager(db);
            db.adjust_balance(beneficiary, asset(temp_debt, GRAPHENE_ASSET_GAS), true);
            temp_supply = temp_debt;
            if (temp_collateral < 0)
            {
                optional<vesting_balance_id_type> new_vbid = db.deposit_lazy_vesting(
                    mortgager_obj.cashback_vb,
                    -temp_collateral,
                    db.get_global_properties().parameters.cashback_vb_period_seconds,
                    mortgager,
                    "cashback_vb",
                    true);
                if (new_vbid.valid())
                    db.modify(mortgager_obj, [&new_vbid](account_object &a) { a.cashback_vb = new_vbid; });
            }
            else
            {
                db.adjust_balance(mortgager, -temp_collateral);
            }
        }
        else
        {
            collateral_for_gas = &db.create<collateral_for_gas_object>([&](collateral_for_gas_object &cgb) {
                cgb.mortgager = mortgager;
                cgb.beneficiary = beneficiary;
                cgb.collateral = collateral;
                cgb.debt = db.estimation_gas(cgb.collateral).amount;
            });
            db.adjust_balance(mortgager, -collateral);
            db.adjust_balance(beneficiary, asset(collateral_for_gas->debt, GRAPHENE_ASSET_GAS), true);
            temp_supply = collateral_for_gas->debt;
        }
        db.modify(GRAPHENE_ASSET_GAS(db).dynamic_asset_data_id(db), [&](asset_dynamic_data_object &data) {
            data.current_supply += temp_supply;
        });
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

lua_map register_scheduler::get_contract_public_data(string name_or_id)
{
    try 
    {
        optional<contract_object> contract = get_contract(name_or_id);
        return contract->contract_data;
    } 
    catch (fc::exception e) 
    {
        LUA_C_ERR_THROW(this->context.mState, e.to_string());
    }
}

static int get_account_contract_data(lua_State *L)
{
    try
    {
        lua_scheduler context(L);
        FC_ASSERT(lua_isstring(L, -2) && lua_istable(L, -1));
        auto name_or_id = lua_scheduler::readTopAndPop<string>(L, -2);
        auto read_list = lua_scheduler::readTopAndPop<lua_table>(L, -1);
        auto current_contract_name = context.readVariable<string>("current_contract");
        auto chainhelper = context.readVariable<register_scheduler *>(current_contract_name, "chainhelper");
        auto &temp_account = chainhelper->get_account(name_or_id);
        auto &contract_udata_index = chainhelper->db.get_index_type<account_contract_data_index>().indices().get<by_account_contract>();
        auto old_account_contract_data_itr = contract_udata_index.find(boost::make_tuple(temp_account.get_id(), chainhelper->contract.get_id()));
        optional<account_contract_data> op_acd;
        if (old_account_contract_data_itr != contract_udata_index.end())
            op_acd = *old_account_contract_data_itr;
        else
            op_acd = account_contract_data();
        if (read_list.v.size() > 0)
        {
            vector<lua_types> stacks;
            lua_map result_table;
            register_scheduler::filter_context(op_acd->contract_data, read_list.v, stacks, &result_table);
            lua_scheduler::Pusher<lua_types>::push(L, lua_table(result_table)).release();
        }
        else
        {
            lua_scheduler::Pusher<lua_types>::push(L, lua_table(op_acd->contract_data)).release();
        }
        return 1;
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(L, e.to_string());
    }
    catch (std::exception e)
    {
        LUA_C_ERR_THROW(L, e.what());
    }
    return 0;
}
static int import_contract(lua_State *L)
{
    const contract_object* temp_contract=nullptr;
    try
    {
        lua_scheduler context(L);
        FC_ASSERT(lua_isstring(L, -1));
        auto name_or_id = lua_scheduler::readTopAndPop<string>(L, -1);
        auto current_contract_name = context.readVariable<string>("current_contract");
        auto chainhelper = context.readVariable<register_scheduler *>(current_contract_name, "chainhelper");
        //auto &temp_contract = chainhelper->get_contract(name_or_id);
        temp_contract = &chainhelper->get_contract(name_or_id);
        auto &temp_contract_code=temp_contract->lua_code_b_id(chainhelper->db);
        auto cbi=context.readVariable<contract_base_info *>(current_contract_name, "contract_base_info");
        auto &baseENV = contract_bin_code_id_type(0)(chainhelper->db);
        FC_ASSERT(lua_getglobal(context.mState, temp_contract->name.c_str())==LUA_TNIL);
        context.new_sandbox(temp_contract->name,baseENV.lua_code_b.data(),baseENV.lua_code_b.size());
        temp_contract->register_function(context,chainhelper, cbi);
        FC_ASSERT(lua_getglobal(context.mState, current_contract_name.c_str()) == LUA_TTABLE);
        if (lua_getfield(context.mState, -1, temp_contract->name.c_str()) == LUA_TTABLE)
            return 1;
        lua_pop(context.mState, 1);
        lua_getglobal(context.mState, temp_contract->name.c_str());
        lua_setfield(context.mState, -2, temp_contract->name.c_str());
        lua_pushnil(context.mState);
        lua_setglobal(context.mState,temp_contract->name.c_str());
        luaL_loadbuffer(context.mState, temp_contract_code.lua_code_b.data(), temp_contract_code.lua_code_b.size(), temp_contract->name.data()); //  lua加载脚本之后会返回一个函数(即此时栈顶的chunk块)，lua_pcall将默认调用此块
        lua_getglobal(context.mState, current_contract_name.c_str());
        lua_getfield(context.mState, -1, temp_contract->name.c_str()); //想要使用的_ENV备用空间
        lua_setupvalue(context.mState, -3, 1);                        //将栈顶变量赋值给栈顶第二个函数的第一个upvalue(当前第二个函数为load返回的函数，第一个upvalue为_ENV),注：upvalue:函数的外部引用变量
        lua_pop(context.mState, 1);
        FC_ASSERT(lua_pcall(context.mState, 0, 0, 0) == 0, "import_contract ${contract}", ("contract", temp_contract->name));
        lua_getglobal(context.mState, current_contract_name.c_str());
        lua_getfield(context.mState, -1, temp_contract->name.c_str());
        return 1;
    }
    catch (fc::exception e)
    {
        if(temp_contract)
        {
           lua_pushnil(L);
           lua_setglobal(L,temp_contract->name.c_str()); 
        }

        wdump((e.to_detail_string()));
        LUA_C_ERR_THROW(L, e.to_string());
    }
    catch (std::runtime_error e)
    {   
        if(temp_contract)
        {
           lua_pushnil(L);
           lua_setglobal(L,temp_contract->name.c_str()); 
        }
        wdump((e.what()));
        LUA_C_ERR_THROW(L, e.what());
    }
    return 0;
} // namespace chain



static int format_vector_with_table(lua_State *L)
{
    try
    {
        lua_scheduler context(L);
        FC_ASSERT(lua_istable(L, -1));
        auto map_list = lua_scheduler::readTopAndPop<lua_table>(L, -1);
        vector<lua_types> temp;
        for(auto itr=map_list.v.begin();itr!=map_list.v.end();itr++)
            temp.emplace_back(itr->second);
        lua_scheduler::Pusher<string>::push(L,fc::json::to_string(fc::variant(temp)) ).release();
        return 1;
    }
    catch (fc::exception e)
    {
        wdump((e.to_detail_string()));
        LUA_C_ERR_THROW(L, e.to_string());
    }
    catch (std::runtime_error e)
    {   
        wdump((e.what()));
        LUA_C_ERR_THROW(L, e.what());
    }
    return 0;
} // namespace chain

void lua_scheduler::chain_function_bind()
{
    registerMember("name", &contract_base_info::name);
    registerMember("id", &contract_base_info::id);
    registerMember("owner", &contract_base_info::owner);
    registerMember("caller", &contract_base_info::caller);
    registerMember("creation_date", &contract_base_info::creation_date);
    registerMember("contract_authority", &contract_base_info::contract_authority);
    registerMember("invoker_contract_id", &contract_base_info::invoker_contract_id);

    registerFunction("log", &register_scheduler::log);
    registerFunction("number_max", &register_scheduler::nummax);
    registerFunction("number_min", &register_scheduler::nummin);
    registerFunction("integer_max", &register_scheduler::integermax);
    registerFunction("integer_min", &register_scheduler::integermin);
    registerFunction("real_time", &register_scheduler::real_time);
    registerFunction("time", &register_scheduler::head_block_time);
    registerFunction("hash256", &register_scheduler::hash256);
    registerFunction("hash512", &register_scheduler::hash512);
    registerFunction("make_memo", &register_scheduler::make_memo);
    registerFunction("make_release", &register_scheduler::make_release);
    registerFunction("random", &register_scheduler::contract_random);
    registerFunction("is_owner", &register_scheduler::is_owner);
    registerFunction("read_chain", &register_scheduler::read_cache);
    registerFunction("write_chain", &register_scheduler::fllush_cache);
    registerFunction("create_nh_asset", &register_scheduler::create_nh_asset);
    registerFunction("adjust_lock_asset", &register_scheduler::adjust_lock_asset);
    registerFunction("nht_describe_change", &register_scheduler::nht_describe_change);
    registerFunction("set_permissions_flag", &register_scheduler::set_permissions_flag);
    registerFunction("set_invoke_share_percent", &register_scheduler::set_invoke_share_percent);
    registerFunction("invoke_contract_function", &register_scheduler::invoke_contract_function);
    registerFunction("change_contract_authority", &register_scheduler::change_contract_authority);
    registerFunction("update_collateral_for_gas", &register_scheduler::update_collateral_for_gas);
    registerFunction("get_contract_public_data", &register_scheduler::get_contract_public_data);
    lua_register(mState, "import_contract", &import_contract);
    lua_register(mState, "get_account_contract_data", &get_account_contract_data);
    lua_register(mState, "format_vector_with_table", &format_vector_with_table);
    registerFunction<register_scheduler, void(string, double, string, bool)>("transfer_from_owner",
                                                                             [](register_scheduler &fc_register, string to, double amount, string symbol, bool enable_logger = false) {
                                                                                 fc_register.transfer_from(fc_register.contract.owner, to, amount, symbol, enable_logger);
                                                                             });
    registerFunction<register_scheduler, void(string, double, string, bool)>("transfer_from_caller",
                                                                             [](register_scheduler &fc_register, string to, double amount, string symbol, bool enable_logger = false) {
                                                                                 fc_register.transfer_from(fc_register.caller, to, amount, symbol, enable_logger);
                                                                             });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_from_owner",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                                                                         auto &token = fc_register.get_nh_asset(token_hash_or_id);
                                                                         auto &account_to = fc_register.get_account(to).id;
                                                                         fc_register.transfer_nht(fc_register.contract.owner, account_to, token, enable_logger);
                                                                     });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_from_caller",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                                                                         auto &token = fc_register.get_nh_asset(token_hash_or_id);
                                                                         auto &account_to = fc_register.get_account(to).id;
                                                                         fc_register.transfer_nht(fc_register.caller, account_to, token, enable_logger);
                                                                     });
    registerFunction<register_scheduler, int64_t(string, string)>("get_account_balance",
                                                                  [](register_scheduler &fc_register, string account, string symbol) -> int64_t {
                                                                      auto &account_ob = fc_register.get_account(account);
                                                                      auto &asset = fc_register.get_asset(symbol);
                                                                      return fc_register.get_account_balance(account_ob.id, asset.id);
                                                                  });
    registerFunction<register_scheduler, void(string, string, bool)>("change_nht_active_by_owner",
                                                                     [](register_scheduler &fc_register, string beneficiary_account, string token_hash_or_id, bool enable_logger = false) {
                auto& token = fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(beneficiary_account).id;
                fc_register.transfer_nht_active(fc_register.contract.owner, account_to, token,enable_logger); });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_active_from_caller",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token = fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;
                fc_register.transfer_nht_active(fc_register.caller, account_to, token,enable_logger); });
    /*            
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_ownership_from_owner",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;
                fc_register.transfer_nht_ownership(fc_register.contract.owner, account_to, token,enable_logger); });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_ownership_from_caller",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;;
                fc_register.transfer_nht_ownership(fc_register.caller, account_to, token,enable_logger); });
    */
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_dealership_from_owner",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;
                fc_register.transfer_nht_dealership(fc_register.contract.owner, account_to, token,enable_logger); });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nht_dealership_from_caller",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;
                fc_register.transfer_nht_dealership(fc_register.caller, account_to, token,enable_logger); });
    registerFunction<register_scheduler, void(string, string, bool, bool)>("set_nht_limit_list",
                                                                           [](register_scheduler &fc_register, string token_hash_or_id, string contract_name_or_ids, bool limit_type, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                fc_register.set_nht_limit_list(fc_register.caller, token, contract_name_or_ids, limit_type, enable_logger); });
    registerFunction<register_scheduler, void(string, string, bool, bool)>("relate_nh_asset",
                                                                           [](register_scheduler &fc_register, string parent_token_hash_or_id, string child_token_hash_or_id, bool relate, bool enable_logger = false) {
                auto& parent =fc_register.get_nh_asset(parent_token_hash_or_id);
                auto& child =fc_register.get_nh_asset(child_token_hash_or_id);
                fc_register.relate_nh_asset(fc_register.caller, parent, child, relate, enable_logger); });
    registerFunction<register_scheduler, string(string, string, string, bool, bool)>("create_nft_asset",
                                                                           [](register_scheduler &fc_register, string owner_id, string world_view, string base_describe, bool dealership_to_contract = false, bool enable_logger = false) {
                auto& owner = fc_register.get_account(owner_id).id;

                if (dealership_to_contract) {
                    auto& dealer = fc_register.contract.owner;
                    return fc_register.create_nft_asset(owner, dealer, world_view, base_describe, enable_logger);
                }
                return fc_register.create_nft_asset(owner, owner, world_view, base_describe, enable_logger); });
    registerFunction<register_scheduler, void(string, bool)>("adjust_lock_nft_asset",
                                                                           [](register_scheduler &fc_register, string token_hash_or_id, bool lock_or_unlock = true) {
                auto& token = fc_register.get_nh_asset(token_hash_or_id);
                fc_register.adjust_lock_nft_asset(token, lock_or_unlock); });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nft_ownership_from_owner",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;
                fc_register.transfer_nft_ownership(fc_register.contract.owner, account_to, token,enable_logger); });
    registerFunction<register_scheduler, void(string, string, bool)>("transfer_nft_ownership_from_caller",
                                                                     [](register_scheduler &fc_register, string to, string token_hash_or_id, bool enable_logger = false) {
                auto& token =fc_register.get_nh_asset(token_hash_or_id);
                auto& account_to = fc_register.get_account(to).id;;
                fc_register.transfer_nft_ownership(fc_register.caller, account_to, token,enable_logger); });

    registerFunction<register_scheduler, string(string)>("get_nft_asset",
                                                                     [](register_scheduler &fc_register, string hash_or_id) {
                auto& token =fc_register.get_nh_asset(hash_or_id);
                try{
                    return  fc::json::to_string(token);
                }
                catch (fc::exception e)
                {
                    LUA_C_ERR_THROW(fc_register.context.mState, e.to_string());
                }});                
}

void contract_object::register_function(lua_scheduler &context, register_scheduler *fc_register, contract_base_info *base_info)const
{
    try
    {
        //context.writeVariable(name,"contract", this);
        context.writeVariable(name, "chainhelper", fc_register);
        context.writeVariable(name, "contract_base_info", base_info);
    }
    FC_CAPTURE_AND_RETHROW()
}

} // namespace chain
} // namespace graphene
