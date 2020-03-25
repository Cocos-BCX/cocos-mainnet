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
        bool not_in_range = false;
        if((percent>=0)&&(percent<=100))
           not_in_range=true;
        FC_ASSERT(not_in_range,"percent should be in range 0-100 ");
        FC_ASSERT(is_owner(), "You`re not the contract`s owner");
        contract_id_type db_index = contract.id;
        db.modify(db_index(db), [&](contract_object &co) {
            co.user_invoke_share_percent = percent;
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

void register_scheduler::set_random_key( string d_str )
{
    try
    {
        public_key_rsa_type rand_key(d_str);
        FC_ASSERT(is_owner(), "You`re not the contract`s owner");
        contract_id_type db_index = contract.id;
        db.modify(db_index(db), [&](contract_object &co) {
            co.random_key = rand_key;
        });
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState,e.to_string());
    }
};

 bool register_scheduler::verify_random_key( string digest_str, string sig_str )
{
    try
    {
        contract_id_type db_index = contract.id;
        auto co = db_index(db);
        public_key_rsa_type  rand_key = co.random_key;
        if ( rand_key != public_key_rsa_type() )
        {
            return rand_key.verify( digest_str, sig_str );
        }
        return false;
    }
    catch (fc::exception e)
    {
        LUA_C_ERR_THROW(this->context.mState,e.to_string());
    }
    return false;
};

} // namespace chain
} // namespace graphene