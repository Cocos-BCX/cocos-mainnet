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
#pragma once

#include <graphene/chain/database.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/crontab_object.hpp>

/*
 * This file provides with() functions which modify the database
 * temporarily, then restore it.  These functions are mostly internal
 * implementation detail of the database.
 *
 * Essentially, we want to be able to use "finally" to restore the
 * database regardless of whether an exception is thrown or not, but there
 * is no "finally" in C++.  Instead, C++ requires us to create a struct
 * and put the finally block in a destructor.  Aagh!
 */

namespace graphene
{
namespace chain
{
namespace detail
{
/**
 * Class used to help the with_skip_flags implementation.
 * It must be defined in this header because it must be
 * available to the with_skip_flags implementation,
 * which is a template and therefore must also be defined
 * in this header.
 */
struct skip_flags_restorer
{
    skip_flags_restorer(node_property_object &npo, uint32_t old_skip_flags)
        : _npo(npo), _old_skip_flags(old_skip_flags)
    {
    }

    ~skip_flags_restorer()
    {
        _npo.skip_flags = _old_skip_flags;
    }

    node_property_object &_npo;
    uint32_t _old_skip_flags; // initialized in ctor
};

/**
 * Class used to help the without_pending_transactions
 * implementation.
 *
 * TODO:  Change the name of this class to better reflect the fact
 * that it restores popped transactions as well as pending transactions.
 */
struct pending_transactions_restorer
{
    pending_transactions_restorer(database &db, std::vector<processed_transaction> &pending_transactions)
        : _db(db)
    {
        _db.log_pending_size();
        _pending_transactions.swap(pending_transactions);
        _db.clear_pending();
    }

    ~pending_transactions_restorer()
    {
        for (const auto &tx : _db._popped_tx)
        {
            try
            {
                if (!_db.is_known_transaction(tx.id()))
                {
                    wlog("_popped_tx : tx${tx}", ("tx", tx));
                    _db._push_transaction(tx, transaction_push_state::pop_block);
                }
            }
            catch (const fc::exception &)
            {
            }
        }
        _db._popped_tx.clear();
        for (const processed_transaction &tx : _pending_transactions)
        {
            try
            {
                if (!_db.is_known_transaction(tx.id()))
                {
                    _db._push_transaction(tx, transaction_push_state::re_push); //nico 重新push被搁置的tx交易  
                    _db.p2p_broadcast(tx);
                }
            }
            catch (const fc::exception &e)
            {
                wlog("Pending transaction became invalid after switching to block ${b}  ${t}", ("b", _db.head_block_id())("t", _db.head_block_time()));
                wlog("The invalid pending transaction caused exception ${e}", ("e", e.to_detail_string()));
            }
        }
        //预计加入约定事件  
        const auto &proposal_expiration_index = _db.get_index_type<proposal_index>().indices().get<by_expiration_and_isruning>().equal_range(boost::make_tuple(true));
        //while (!proposal_expiration_index.empty() && proposal_expiration_index.begin()->expiration_time <= _db.head_block_time())
        for (auto itr = proposal_expiration_index.first; itr != proposal_expiration_index.second && itr->expiration_time <= _db.head_block_time(); itr++)
        {
            try
            {
                if (!_db.is_known_transaction(itr->proposed_transaction.id()))
                {
                    auto tx = signed_transaction(itr->proposed_transaction);
                    tx.agreed_task = std::make_pair(itr->trx_hash, itr->id);
                    _db._push_transaction(tx, transaction_push_state::from_net);
                }
            }
            catch (const fc::exception &e)
            {
                elog("Failed to apply proposed transaction on its expiration. Deleting it.\n${proposal}\n${error}",
                     ("proposal", *itr)("error", e.to_detail_string()));
            }
        }

        //Convert crontabs that can be executed to agreed tasks
        const auto &crontab_suspend_status_index = _db.get_index_type<crontab_index>().indices().get<by_susupend_status_and_next_execute_time>();
        const auto &range = crontab_suspend_status_index.equal_range(false);
        for (auto itr = range.first; itr != range.second && itr->next_execte_time <= _db.head_block_time(); itr++)
        {
            try
            {
                if (!_db.is_known_transaction(itr->timed_transaction.id()) && itr->allow_execution)
                {
                    auto tx = signed_transaction(itr->timed_transaction);
                    tx.agreed_task = std::make_pair(itr->trx_hash, itr->id);
                    _db._push_transaction(tx, transaction_push_state::from_net);
                }
                else
                {
                    elog("This crontab is not authorized to execute.\n${crontab}", ("crontab", *itr));
                }
            }
            catch (const fc::exception &e)
            {
                elog("Failed to apply timed transaction on its scheduled execute times.\n${crontab}\n${error}",
                     ("crontab", *itr)("error", e.to_detail_string()));
            }
        }
    }

    database &_db;
    std::vector<processed_transaction> _pending_transactions;
};

/**
 * Set the skip_flags to the given value, call callback,
 * then reset skip_flags to their previous value after
 * callback is done.
 */
template <typename Lambda>
void with_skip_flags(
    database &db,
    uint32_t skip_flags,
    Lambda callback)
{
    node_property_object &npo = db.node_properties();
    skip_flags_restorer restorer(npo, npo.skip_flags);
    npo.skip_flags = skip_flags;
    callback();
    return;
}

/**
 * Empty pending_transactions, call callback,
 * then reset pending_transactions after callback is done.
 *
 * Pending transactions which no longer validate will be culled.
 */
template <typename Lambda>
void without_pending_transactions(
    database &db,
    std::vector<processed_transaction> &pending_transactions,
    Lambda callback)
{
    pending_transactions_restorer restorer(db, pending_transactions); //nico 在pending_transactions_restorer的析构函数中将继续应用被搁置的交易\
                                                                                         被搁置的应用中将重新回到pending池中
    callback();
    return;
}

} // namespace detail
} // namespace chain
} // namespace graphene
