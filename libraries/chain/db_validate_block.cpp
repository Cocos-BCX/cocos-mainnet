#include <graphene/chain/database.hpp>
#include <graphene/chain/db_with.hpp>
#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/block_summary_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/operation_history_object.hpp>

#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/evaluator.hpp>

#include <graphene/chain/temporary_authority.hpp>

#include <fc/smart_ref_impl.hpp>

#include <boost/thread/thread.hpp>
namespace graphene
{
namespace chain
{

/**
 * Push block "may fail" in which case every partial change is unwound.  After
 * push block is successful the block is appended to the chain database on disk.
 *
 * @return true if we switched forks as a result of this push.
 */
bool database::validate_block(signed_block &new_block, const fc::ecc::private_key &block_signing_private_key, uint32_t skip)
{
    //  idump((new_block.block_num())(new_block.id())(new_block.timestamp)(new_block.previous));
    bool result;
    if (new_block.block_num()>pause_point_num&&pause_point_num)
    {
        FC_THROW("The pause point has been reached. Maybe you should close the node program.");
    }
    detail::with_skip_flags(*this, skip, [&]() {
        detail::without_pending_transactions(*this, _pending_tx,
                                             [&]() {
                                                 result = _validate_block(new_block, block_signing_private_key);
                                             });
    });
    return result;
}

bool database::_validate_block(signed_block &new_block, const fc::ecc::private_key &block_signing_private_key)
{
    try
    {
        uint32_t skip = get_node_properties().skip_flags;
        try
        {
            auto session = _undo_db.start_undo_session();
            FC_ASSERT(head_block_id() == new_block.previous, "", ("head_block_id", head_block_id())("next.prev", new_block.previous));
            FC_ASSERT(head_block_time() < new_block.timestamp, "", ("head_block_time", head_block_time())("next", new_block.timestamp)("blocknum", new_block.block_num()));
            const witness_object &signing_witness = new_block.witness(*this);
            const auto &global_props = get_global_properties();
            const auto &dynamic_global_props = get<dynamic_global_property_object>(dynamic_global_property_id_type());
            bool maint_needed = (dynamic_global_props.next_maintenance_time <= new_block.timestamp);
            try_apply_block(new_block, skip);
            new_block.transaction_merkle_root = new_block.calculate_merkle_root() /*checking_transactions_hash()*/;
            new_block.sign(block_signing_private_key);
            new_block.block_id=new_block.make_id();
            if (!(skip & skip_fork_db))
                shared_ptr<fork_item> new_head = _fork_db.push_block(new_block); 
            update_global_dynamic_data(new_block);
            update_signing_witness(signing_witness, new_block);
            update_last_irreversible_block();
            // Are we at the maintenance interval?
            if (maint_needed)
                perform_chain_maintenance(new_block, global_props);

            create_block_summary(new_block);
            clear_expired_transactions();
            clear_expired_nh_asset_orders();
            clear_expired_proposals();
            clear_expired_orders();
            clear_expired_timed_task();
            update_expired_feeds();
            clear_expired_active();
            update_maintenance_flag(maint_needed);
            update_witness_schedule();
            if (!_node_property_object.debug_updates.empty())
                apply_debug_updates();

            // notify observers that the block has been applied
            applied_block(new_block); // applied_block
            _applied_ops.clear();
            notify_changed_objects(); // 消息通知对象改变
            _block_id_to_block.store(new_block.block_id, new_block);

            session.commit();
        }
        catch (const fc::exception &e)
        {
            elog("Failed to push new block:\n${e}", ("e", e.to_detail_string()));
            _fork_db.remove(new_block.block_id);
            throw;
        }

        return false;
    }
    FC_CAPTURE_AND_RETHROW((new_block))
}

void database::try_apply_block(signed_block &next_block, uint32_t skip)
{
    auto block_num = next_block.block_num();
    if (_checkpoints.size() && _checkpoints.rbegin()->second != block_id_type())
    {
        auto itr = _checkpoints.find(block_num);
        if (itr != _checkpoints.end())
            FC_ASSERT(next_block.block_id == itr->second, "Block did not match checkpoint", ("checkpoint", *itr)("block_id", next_block.block_id));

        if (_checkpoints.rbegin()->first >= block_num)
            skip = ~0; // WE CAN SKIP ALMOST EVERYTHING
    }

    detail::with_skip_flags(*this, skip, [&]() {
        _try_apply_block(next_block);
    });
    return;
}

void database::_try_apply_block(signed_block &next_block)
{
    try
    {
        uint32_t next_block_num = next_block.block_num();
        uint32_t skip = get_node_properties().skip_flags;
        _applied_ops.clear();
        _current_block_num = next_block_num;
        _current_trx_in_block = 0;
        std::vector<std::pair<tx_hash_type, processed_transaction>> temp;
        temp.reserve(next_block.transactions.size());
        temp.swap(next_block.transactions);
        next_block.transactions.clear();
        bool canstop = false, apply_transactions_thread_is_stop = false;
        fc::microseconds start = fc::time_point::now().time_since_epoch();
        auto maximum_block_size = get_global_properties().parameters.maximum_block_size;
        boost::thread apply_transactions_thread([&]() {
            for (auto itr = temp.begin(); itr != temp.end() && !canstop;) // 验证区块中的tx
            {
                if (fc::raw::pack_size(next_block) > maximum_block_size)
                    break;
                auto session = _undo_db.start_undo_session();
                try
                {
                    itr->second = apply_transaction(itr->second, skip, transaction_apply_mode::production_block_mode); // 应用交易transaction
                }
                catch (fc::exception &e)
                {
                    ++itr;
                    session.undo();
                    continue;
                }
                if (!(skip & skip_block_size_check))
                {
                    if (fc::raw::pack_size(*itr) > maximum_block_size)
                    {
                        ++itr;
                        session.undo();
                        continue;
                    }
                }
                next_block.transactions.emplace_back(*itr);
                session.merge();
                ++itr;
                ++_current_trx_in_block;
            }
            apply_transactions_thread_is_stop = true;
        });
        do
        {
            usleep(100);
            if (fc::time_point::now().time_since_epoch().count() - start.count() > block_interval() * 1000000)
                canstop = true;
        } while (!apply_transactions_thread_is_stop);
    }
    FC_CAPTURE_AND_RETHROW((next_block.block_num()))
}

} // namespace chain
} // namespace graphene