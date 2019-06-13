
#include <graphene/chain/database.hpp>
#include <graphene/chain/temporary_authority_evaluator.hpp>
#include <graphene/chain/temporary_authority.hpp>
#include <graphene/chain/account_object.hpp>

namespace graphene
{
namespace chain
{

void_result temporary_authority_change_evaluator::do_evaluate(const operation_type &o)
{
    database &d = db();
    FC_ASSERT(o.expiration_time > d.head_block_time());
    //account_object signer=o.owner(d);

    //nico 验证签名是否是临时授权，临时授权不能修改临时授权
    //flat_set<public_key_type> sigkeys=trx_state->_trx->get_signature_keys(d.get_chain_id());
    if (trx_state->run_mode != transaction_apply_mode::apply_block_mode && trx_state->run_mode != transaction_apply_mode::production_block_mode)
    {
        auto &index = d.get_index_type<temporary_active_index>().indices().get<by_account_id_type>();
        for (auto itr = index.find(o.owner); itr != index.end(); itr++)
        {
            for (auto itr_sig_key = trx_state->sigkeys.begin(); itr_sig_key != trx_state->sigkeys.end(); itr_sig_key++)
            {
                for (auto temporary_key = itr->temporary_active.begin(); temporary_key != itr->temporary_active.end(); temporary_key++)
                    FC_ASSERT(*itr_sig_key != temporary_key->first, " signature secret key (${key}) may be a temporary permission!",
                              ("key", string(*itr_sig_key)));
            }
        }
    }
    return void_result();
};
void_result temporary_authority_change_evaluator::do_apply(const operation_type &o)
{

    database &d = db();

    auto &index = d.get_index_type<temporary_active_index>().indices().get<by_account_and_describe>();
    auto itr = index.find(boost::make_tuple(o.owner, o.describe));

    if (itr == index.end())
    {
        d.create<temporary_active_object>([&](temporary_active_object &c) {
            c.owner = o.owner;
            c.describe = o.describe;
            c.temporary_active = o.temporary_active;
            c.expiration_time = o.expiration_time;
        });
    }
    else
    {
        d.modify<temporary_active_object>(*itr, [&](temporary_active_object &c) {
            c.owner = o.owner;
            c.describe = o.describe;
            c.temporary_active = o.temporary_active;
            c.expiration_time = o.expiration_time;
        });
    }

    return void_result();
};

} // namespace chain
} // namespace graphene