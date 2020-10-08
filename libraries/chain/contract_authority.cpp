#include <graphene/chain/contract_object.hpp>
#include <graphene/chain/contract_function_register_scheduler.hpp>
namespace graphene
{
namespace chain
{

bool register_scheduler::is_owner()
{
    return caller == contract.owner;
}
void register_scheduler::set_permissions_flag(bool flag)
{
    try
    {
        FC_ASSERT(is_owner(), "You`re not the contract`s owner");
        contract_id_type db_index = contract.id;
        db.modify(db_index(db), [&](contract_object &co) {
            co.check_contract_authority = flag;
        });
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState,e.to_string());
    }
};

void register_scheduler::set_invoke_share_percent(double percent)
{
    try
    {
        FC_ASSERT(caller == contract.owner, "You`re not the contract`s owner");
        uint32_t n_percent = static_cast<uint32_t>(percent);
        FC_ASSERT(((n_percent >= 0) && (n_percent <= 100)), "percent should be in range 0-100 ");
        contract_id_type db_index = contract.id;
        db.modify(db_index(db), [&](contract_object &co) {
            co.user_invoke_share_percent = n_percent;
        });
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState,e.to_string());
    }
};

void register_scheduler::change_contract_authority(string authority)
{
    try
    {
        FC_ASSERT(is_owner(), "You`re not the contract`s owner");
        auto new_authority= public_key_type(authority);
        contract_id_type db_index = contract.id;
        db.modify(db_index(db), [&](contract_object &co) {
            co.contract_authority = new_authority;
        });
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState,e.to_string());
    }
}

} // namespace chain
} // namespace graphene