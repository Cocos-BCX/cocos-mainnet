/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <graphene/chain/database.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/witness_object.hpp>

namespace graphene
{
namespace chain
{

asset database::get_balance(account_id_type owner, asset_id_type asset_id) const
{
    auto &index = get_index_type<account_balance_index>().indices().get<by_account_asset>();
    auto itr = index.find(boost::make_tuple(owner, asset_id));
    if (itr == index.end())
        return asset(0, asset_id);
    return itr->get_balance();
}

asset database::get_balance(const account_object &owner, const asset_object &asset_obj) const
{
    return get_balance(owner.get_id(), asset_obj.get_id());
}

string database::to_pretty_string(const asset &a) const
{
    return a.asset_id(*this).amount_to_pretty_string(a.amount);
}

void database::adjust_balance(account_id_type account, asset delta)
{
    try
    {
        if (delta.amount == share_type(0))
            return;

        auto &index = get_index_type<account_balance_index>().indices().get<by_account_asset>();
        auto itr = index.find(boost::make_tuple(account, delta.asset_id));
        if (itr == index.end())
        {
            FC_ASSERT(delta.amount > share_type(0), "Insufficient Balance: ${a}'s balance of ${b} is less than required ${r}",
                      ("a", account(*this).name)("b", to_pretty_string(asset(0, delta.asset_id)))("r", to_pretty_string(-delta)));
            this->create<account_balance_object>([account, &delta](account_balance_object &b) {
                b.owner = account;
                b.asset_type = delta.asset_id;
                b.balance = delta.amount.value;
            });
        }
        else
        {
            if (delta.amount < share_type(0))
            {
                FC_ASSERT(itr->get_balance() >= -delta, "Insufficient Balance: ${a}'s balance of ${b} is less than required ${r}",
                          ("a", account(*this).name)("b", to_pretty_string(itr->get_balance()))("r", to_pretty_string(-delta)));
                auto account_ob = account(*this);
                optional<share_type> locked = account_ob.contract_asset_locked.locked_total[delta.asset_id];
                if (locked)
                    FC_ASSERT(locked->value>=0&&itr->get_balance() + delta >=asset(locked->value,delta.asset_id), "Some assets are locked and cannot be transferred.asset_id:${asset_id},amount:${amount}",
                              ("asset_id", delta.asset_id)("amount", locked->value));
            }
            this->modify(*itr, [delta](account_balance_object &b) {
                b.adjust_balance(delta);
            });
        }
    }
    FC_CAPTURE_AND_RETHROW((account)(delta))
}

optional<vesting_balance_id_type> database::deposit_lazy_vesting(
    const optional<vesting_balance_id_type> &ovbid,
    share_type amount, uint32_t req_vesting_seconds,
    account_id_type req_owner,
    bool require_vesting)
{
    if (amount == share_type(0))
        return optional<vesting_balance_id_type>();

    fc::time_point_sec now = head_block_time();

    while (true)
    {
        if (!ovbid.valid())
            break;
        const vesting_balance_object &vbo = (*ovbid)(*this);
        if (vbo.owner != req_owner)
            break;
        if (vbo.policy.which() != vesting_policy::tag<cdd_vesting_policy>::value)
            break;
        if (vbo.policy.get<cdd_vesting_policy>().vesting_seconds != req_vesting_seconds)
            break;
        modify(vbo, [&](vesting_balance_object &_vbo) {
            if (require_vesting)
                _vbo.deposit(now, amount);
            else
                _vbo.deposit_vested(now, amount);
        });
        return optional<vesting_balance_id_type>();
    }

    const vesting_balance_object &vbo = create<vesting_balance_object>([&](vesting_balance_object &_vbo) {
        _vbo.owner = req_owner;
        _vbo.balance = amount;

        cdd_vesting_policy policy;
        policy.vesting_seconds = req_vesting_seconds;
        policy.coin_seconds_earned = require_vesting ? 0 : amount.value * policy.vesting_seconds;
        policy.coin_seconds_earned_last_update = now;

        _vbo.policy = policy;
    });

    return vbo.id;
}
//  用户网络费用返现
void database::deposit_cashback(const account_object &acct, share_type amount, bool require_vesting)
{
    // If we don't have a VBO, or if it has the wrong maturity
    // due to a policy change, cut it loose.

    if (amount == share_type(0))
        return;

    if (acct.get_id() == GRAPHENE_COMMITTEE_ACCOUNT || acct.get_id() == GRAPHENE_WITNESS_ACCOUNT ||
        //acct.get_id() == GRAPHENE_RELAXED_COMMITTEE_ACCOUNT || 
        acct.get_id() == GRAPHENE_NULL_ACCOUNT ||acct.get_id() == GRAPHENE_TEMP_ACCOUNT)
    {
        // The blockchain's accounts do not get cashback; it simply goes to the reserve pool.
        // 这些链上的公共账户不会拥有返现，该部分资金将转移到储备池，操作:减少当前资产流通量
        modify(get(asset_id_type()).dynamic_asset_data_id(*this), [amount](asset_dynamic_data_object &d) {
            d.current_supply -= amount; // 修改当前供应量
        });
        return;
    }

    optional<vesting_balance_id_type> new_vbid = deposit_lazy_vesting(
        acct.cashback_vb,
        amount,
        get_global_properties().parameters.cashback_vesting_period_seconds,
        acct.id,
        require_vesting);

    if (new_vbid.valid())
    {
        modify(acct, [&](account_object &_acct) {
            _acct.cashback_vb = *new_vbid;
        });
    }

    return;
}

// 见证人费用支付

void database::deposit_witness_pay(const witness_object &wit, share_type amount)
{
    if (amount == share_type(0))
        return;

    optional<vesting_balance_id_type> new_vbid = deposit_lazy_vesting(
        wit.pay_vb,
        amount,
        get_global_properties().parameters.witness_pay_vesting_seconds,
        wit.witness_account,
        true);

    if (new_vbid.valid())
    {
        modify(wit, [&](witness_object &_wit) {
            _wit.pay_vb = *new_vbid;
        });
    }

    return;
}

} // namespace chain
} // namespace graphene
