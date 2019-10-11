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
#include <graphene/chain/asset_evaluator.hpp>
#include <graphene/chain/crontab_evaluator.hpp>
#include <graphene/chain/crontab_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/db_notify.hpp>

#include <fc/smart_ref_impl.hpp>

namespace graphene
{
namespace chain
{

void_result crontab_create_evaluator::do_evaluate(const operation_type &o)
{
    try
    {
        const database &d = db();
        FC_ASSERT(o.start_time > d.head_block_time());
        FC_ASSERT(o.start_time + o.scheduled_execute_times * o.execute_interval <= d.head_block_time() + 2592000, "The crontab execution period can't exceed 3 months");
        flat_set<account_id_type> impacted;

        for (const op_wrapper &op : o.crontab_ops)
        {
            _timed_trx.operations.emplace_back(op.op);
            graphene::chain::operation_get_impacted_accounts( op.op, impacted );
        }
        // Verify that the impacted account exists
        for( const account_id_type &id : impacted )
            FC_ASSERT(nullptr != d.find(id), "can't find account:${account}",("account",id));
        
        _timed_trx.set_reference_block(d.head_block_id());
        _timed_trx.validate();

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

object_id_result crontab_create_evaluator::do_apply(const operation_type &o)
{
    try
    {
        database &d = db();
        auto next_id = d.get_index_type<crontab_index>().get_next_id();
        auto &gbp = d.get_global_properties().parameters;
        const auto &time_now = d.head_block_time();
        crontab_object crontab;
        crontab.trx_hash = trx_state->_trx->hash();
        crontab.task_owner = o.crontab_creator;
        crontab.next_execte_time = o.start_time;
        crontab.expiration_time = crontab.next_execte_time + o.execute_interval * o.scheduled_execute_times;
        _timed_trx.extensions=vector<string>{string(next_id)};
        _timed_trx.expiration = crontab.next_execte_time + std::min(gbp.assigned_task_life_cycle, (uint32_t)7200);
        crontab.timed_transaction = _timed_trx;
        crontab.execute_interval = o.execute_interval;
        crontab.scheduled_execute_times = o.scheduled_execute_times;
        crontab.already_execute_times = 0;
        // Verify that if the crontab is authorized to execute
        if(crontab.is_authorized_to_execute(d))
        {
            crontab.allow_execution=true;
        }else
        {
             FC_THROW("Crontab has no authorize to execute.");
        }
        FC_ASSERT(d.create<crontab_object>([&](crontab_object &cr) {cr=crontab;}).id==next_id);
        return next_id;
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result crontab_cancel_evaluator::do_evaluate(const operation_type &o)
{
    try
    {
        database &d = db();

        _crontab = &o.task(d);

        // Verify that if the operator is crontab's owner        
        FC_ASSERT(o.fee_paying_account == _crontab->task_owner, "You are not the crontab's owner.");

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result crontab_cancel_evaluator::do_apply(const operation_type &o)
{
    try
    {
        db().remove(*_crontab);

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result crontab_recover_evaluator::do_evaluate(const operation_type &o)
{
    try
    {
        database &d = db();
        _crontab = &o.crontab(d);
        FC_ASSERT(o.restart_time > d.head_block_time());
        // Verify that if the operator is crontab's owner
        FC_ASSERT(o.crontab_owner == _crontab->task_owner, "You are not the crontab's owner.");

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result crontab_recover_evaluator::do_apply(const operation_type &o)
{
    try
    {
        database &d = db();
        auto &gbp = d.get_global_properties().parameters;
        d.modify(*_crontab, [&](crontab_object &crontab) {
                crontab.is_suspended = false;
                crontab.continuous_failure_times = 0;
                crontab.next_execte_time = o.restart_time;
                crontab.expiration_time = crontab.next_execte_time + crontab.execute_interval * (crontab.scheduled_execute_times - crontab.already_execute_times);
                crontab.timed_transaction.expiration = crontab.next_execte_time + std::min(gbp.assigned_task_life_cycle, (uint32_t)7200);
            });

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}


} // namespace chain
} // namespace graphene
