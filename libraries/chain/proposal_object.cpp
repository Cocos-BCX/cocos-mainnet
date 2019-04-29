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
#include <graphene/chain/proposal_object.hpp>

namespace graphene
{
namespace chain
{

bool proposal_object::is_authorized_to_execute(database &db) const
{
    //transaction_evaluation_state dry_run_eval(&db);

    try
    {
        graphene::chain::verify_authority(proposed_transaction.operations,
                                          available_key_approvals,
                                          [&](account_id_type id) { return &id(db).active; },
                                          [&](account_id_type id) { return &id(db).owner; },
                                          db.get_global_properties().parameters.max_authority_depth,
                                          true, /* allow committeee */
                                          available_active_approvals,
                                          available_owner_approvals);
        // If the proposal has a review period, don't bother attempting to authorize/execute it.
        // Proposals with a review period may never be executed except at their expiration.
    }
    catch (const fc::exception &e)
    {
        //idump((available_active_approvals));
        //wlog((e.to_detail_string()));
        return false;
    }
    return true;
}

void required_approval_index::object_inserted(const object &obj)
{
    assert(dynamic_cast<const proposal_object *>(&obj));
    const proposal_object &p = static_cast<const proposal_object &>(obj);

    for (const auto &a : p.required_active_approvals)
        _account_to_proposals[a].insert(p.id);
    for (const auto &a : p.required_owner_approvals)
        _account_to_proposals[a].insert(p.id);
    for (const auto &a : p.available_active_approvals)
        _account_to_proposals[a].insert(p.id);
    for (const auto &a : p.available_owner_approvals)
        _account_to_proposals[a].insert(p.id);
}

void required_approval_index::remove(account_id_type a, proposal_id_type p)
{
    auto itr = _account_to_proposals.find(a);
    if (itr != _account_to_proposals.end())
    {
        itr->second.erase(p);
        if (itr->second.empty())
            _account_to_proposals.erase(itr->first);
    }
}

void required_approval_index::object_removed(const object &obj)
{
    assert(dynamic_cast<const proposal_object *>(&obj));
    const proposal_object &p = static_cast<const proposal_object &>(obj);

    for (const auto &a : p.required_active_approvals)
        remove(a, p.id);
    for (const auto &a : p.required_owner_approvals)
        remove(a, p.id);
    for (const auto &a : p.available_active_approvals)
        remove(a, p.id);
    for (const auto &a : p.available_owner_approvals)
        remove(a, p.id);
}

} // namespace chain
} // namespace graphene
