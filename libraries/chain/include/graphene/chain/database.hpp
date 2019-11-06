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
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/node_property_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/fork_database.hpp>
#include <graphene/chain/block_database.hpp>
#include <graphene/chain/genesis_state.hpp>
#include <graphene/chain/evaluator.hpp>

#include <graphene/db/object_database.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/simple_index.hpp>
#include <fc/signals.hpp>

#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/chain/file_object.hpp>

#include <fc/log/logger.hpp>
#include <boost/thread/recursive_mutex.hpp> //nico add ::允许递归锁
#include <map>
#include <lua_extern.hpp>
#include <graphene/chain/protocol/lua_scheduler.hpp>

namespace graphene
{
namespace chain
{
using graphene::db::abstract_object;
using graphene::db::object;
class op_evaluator;
class transaction_evaluation_state;
class lua_scheduler;
struct budget_record;

/**
    *   @class database
    *   @brief tracks the blockchain state in an extensible manner
    */
class database : public db::object_database
{
  public:
    //////////////////// db_management.cpp ////////////////////

    database(const fc::path& data_dir);
    ~database();

    enum validation_steps
    {
        skip_nothing = 0,
        skip_witness_signature = 1 << 0,       ///< used while reindexing
        skip_transaction_signatures = 1 << 1,  ///< used by non-witness nodes
        skip_transaction_dupe_check = 1 << 2,  ///< used while reindexing
        skip_fork_db = 1 << 3,                 ///< used while reindexing
        skip_block_size_check = 1 << 4,        ///< used when applying locally generated transactions
        skip_tapos_check = 1 << 5,             ///< used while reindexing -- note this skips expiration check as well
        skip_authority_check = 1 << 6,         ///< used while reindexing -- disables any checking of authority on transactions
        skip_merkle_check = 1 << 7,            ///< used while reindexing
        skip_assert_evaluation = 1 << 8,       ///< used while reindexing
        skip_undo_history_check = 1 << 9,      ///< used while reindexing
        skip_witness_schedule_check = 1 << 10, ///< used while reindexing
        skip_validate = 1 << 11                ///< used prior to checkpoint, skips validate() call on transaction
    };

    /**
          * @brief Open a database, creating a new one if necessary
          *
          * Opens a database in the specified directory. If no initialized database is found, genesis_loader is called
          * and its return value is used as the genesis state when initializing the new database
          *
          * genesis_loader will not be called if an existing database is found.
          *
          * @param data_dir Path to open or create database in
          * @param genesis_loader A callable object which returns the genesis state to initialize new databases on
          * @param db_version a version string that changes when the internal database format and/or logic is modified
          */
    void open(
        const fc::path &data_dir,
        std::function<genesis_state_type()> genesis_loader,
        const std::string &db_version);

   void open(
        const fc::path &data_dir,
        std::function<genesis_state_type()> genesis_loader,
        const std::string &db_version,int roll_back_at_height);
    /**
          * @brief Rebuild object graph from block history and open detabase
          *
          * This method may be called after or instead of @ref database::open, and will rebuild the object graph by
          * replaying blockchain history. When this method exits successfully, the database will be open.
          */
    void reindex(fc::path data_dir);
    void reindex(fc::path data_dir,int roll_back_at_height);
    /**
          * @brief wipe Delete database from disk, and potentially the raw chain as well.
          * @param include_blocks If true, delete the raw chain as well as the database.
          *
          * Will close the database before wiping. Database will be closed when this function returns.
          */
    void wipe(const fc::path &data_dir, bool include_blocks);
    void close(bool rewind = true);

    //////////////////// db_block.cpp ////////////////////

    /**
          *  @return true if the block is in our fork DB or saved to disk as
          *  part of the official chain, otherwise return false
          */
    bool is_known_block(const block_id_type &id) const;
    bool is_known_transaction(const transaction_id_type &id) const;
    block_id_type get_block_id_for_num(uint32_t block_num) const;
    optional<signed_block> fetch_block_by_id(const block_id_type &id) const;
    optional<signed_block> fetch_block_by_number(uint32_t num) const;
    const signed_transaction &get_recent_transaction(const string &trx_id) const;
    std::vector<block_id_type> get_block_ids_on_fork(block_id_type head_of_fork) const;

    /**
          *  Calculate the percent of block production slots that were missed in the
          *  past 128 blocks, not including the current block.
          */
    uint32_t witness_participation_rate() const;

    void add_checkpoints(const flat_map<uint32_t, block_id_type> &checkpts);
    const flat_map<uint32_t, block_id_type> get_checkpoints() const { return _checkpoints; }
    bool before_last_checkpoint() const;

    bool push_block(const signed_block &b, uint32_t skip = skip_nothing);
    processed_transaction push_transaction(const signed_transaction &trx, uint32_t skip = skip_nothing, transaction_push_state push_state = transaction_push_state::from_me);
    bool _push_block(const signed_block &b);
    processed_transaction _push_transaction(const signed_transaction &trx, transaction_push_state push_state);

    bool validate_block(signed_block &b, const fc::ecc::private_key &block_signing_private_key, uint32_t skip = skip_authority_check); //nico 在验证区块的时候，跳过对tx签名的检查(tx签名验证在push模式已经查验过了)
    bool _validate_block(signed_block &b, const fc::ecc::private_key &block_signing_private_key);

    ///@throws fc::exception if the proposed transaction fails to apply.
    processed_transaction push_proposal(const proposal_object &proposal);

    signed_block generate_block(
        const fc::time_point_sec when,
        witness_id_type witness_id,
        const fc::ecc::private_key &block_signing_private_key,
        uint32_t skip);
    signed_block _generate_block(
        const fc::time_point_sec when,
        witness_id_type witness_id,
        const fc::ecc::private_key &block_signing_private_key);

    void pop_block();
    void clear_pending();

    /**
          *  This method is used to track appied operations during the evaluation of a block, these
          *  operations should include any operation actually included in a transaction as well
          *  as any implied/virtual operations that resulted, such as filling an order.  The
          *  applied operations is cleared after applying each block and calling the block
          *  observers which may want to index these operations.
          *
          *  @return the op_id which can be used to set the result after it has finished being applied.
          */
    uint32_t push_applied_operation(const operation &op);
    void set_applied_operation_result(uint32_t op_id, const operation_result &r);
    const vector<optional<operation_history_object>> &get_applied_operations() const;

    string to_pretty_string(const asset &a) const;

    /**
          *  This signal is emitted after all operations and virtual operation for a
          *  block have been applied but before the get_applied_operations() are cleared.
          *
          *  You may not yield from this callback because the blockchain is holding
          *  the write lock and may be in an "inconstant state" until after it is
          *  released.
          */
    fc::signal<void(const signed_block &)> applied_block;
    /*******************************************************nico add****************************************************/
    uint32_t pause_point_num=0;
    uint16_t _message_cache_size_limit=0;
    flat_set<vote_id_type> concerned_candidates; 
    //Whether to deduce in verification mode
    bool deduce_in_verification_mode=false;
    void clear_expired_active();
    void clear_expired_timed_task();
    bool log_pending_size();
    asset estimation_gas(const asset& delta_collateral);
    void assert_balance(account_object a,const asset& target);
    void set_concerned_candidates(const flat_set<vote_id_type> candidates ){concerned_candidates=candidates; }
    void set_message_cache_size_limit(uint16_t message_cache_size_limit);
    void set_deduce_in_verification_mode(bool flag){deduce_in_verification_mode=flag;}

    // 执行定时任务
    fc::signal<void(const uint32_t participating, bool maybe_allow_transaction)> allowe_continue_transaction;
    fc::signal<void(const signed_transaction tx)> p2p_broadcast;
    fc::signal<uint64_t() > get_message_send_cache_size;
    template <class T>
    optional<T> maybe_id(const string &name_or_id)
    {
        if (std::isdigit(name_or_id.front()))
        {
            try
            {
                return fc::variant(name_or_id).as<T>();
            }
            catch (const fc::exception &)
            {
            }
        }
        return optional<T>();
    }
    const account_object& get_account(const string &name_or_id);
    const transaction_in_block_info &get_transaction_in_block_info(const string &trx_id) const;
    void try_apply_block(signed_block &next_block, uint32_t skip = skip_nothing);
    void _try_apply_block(signed_block &next_block);
    optional<file_object> lookup_file(const string &file_name_or_ids) const;
    graphene::chain::lua_scheduler &get_luaVM() { return luaVM; };
    void initialize_luaVM();
    void initialize_baseENV();
    
    /*******************************************************nico end****************************************************/

    /**
          * This signal is emitted any time a new transaction is added to the pending
          * block state.
          */
    fc::signal<void(const signed_transaction &)> on_pending_transaction;

    /**
          *  Emitted After a block has been applied and committed.  The callback
          *  should not yield and should execute quickly.
          */
    fc::signal<void(const vector<object_id_type> &, const flat_set<account_id_type> &)> new_objects;

    /**
          *  Emitted After a block has been applied and committed.  The callback
          *  should not yield and should execute quickly.
          */
    fc::signal<void(const vector<object_id_type> &, const flat_set<account_id_type> &)> changed_objects;

    /** this signal is emitted any time an object is removed and contains a
          * pointer to the last value of every object that was removed.
          */
    fc::signal<void(const vector<object_id_type> &, const vector<const object *> &, const flat_set<account_id_type> &)> removed_objects;

    //////////////////// db_witness_schedule.cpp ////////////////////

    /**
          * @brief Get the witness scheduled for block production in a slot.
          *
          * slot_num always corresponds to a time in the future.
          *
          * If slot_num == 1, returns the next scheduled witness.
          * If slot_num == 2, returns the next scheduled witness after
          * 1 block gap.
          *
          * Use the get_slot_time() and get_slot_at_time() functions
          * to convert between slot_num and timestamp.
          *
          * Passing slot_num == 0 returns GRAPHENE_NULL_WITNESS
          */
    witness_id_type get_scheduled_witness(uint32_t slot_num) const;

    /**
          * Get the time at which the given slot occurs.
          *
          * If slot_num == 0, return time_point_sec().
          *
          * If slot_num == N for N > 0, return the Nth next
          * block-interval-aligned time greater than head_block_time().
          */
    fc::time_point_sec get_slot_time(uint32_t slot_num) const;

    /**
          * Get the last slot which occurs AT or BEFORE the given time.
          *
          * The return value is the greatest value N such that
          * get_slot_time( N ) <= when.
          *
          * If no such N exists, return 0.
          */
    uint32_t get_slot_at_time(fc::time_point_sec when) const;

    void update_witness_schedule();

    //////////////////// db_getter.cpp ////////////////////

    const chain_id_type &get_chain_id() const;
    const asset_object &get_core_asset() const;
    const chain_property_object &get_chain_properties() const;
    const global_property_object &get_global_properties() const;
    const dynamic_global_property_object &get_dynamic_global_properties() const;
    const node_property_object &get_node_properties() const;
    const fee_schedule &current_fee_schedule() const;

    time_point_sec head_block_time() const;
    uint32_t head_block_num() const;
    block_id_type head_block_id() const;
    witness_id_type head_block_witness() const;

    decltype(chain_parameters::block_interval) block_interval() const;

    node_property_object &node_properties();

    uint32_t last_non_undoable_block_num() const;
    //////////////////// db_init.cpp ////////////////////

    void initialize_evaluators();
    /// Reset the object graph in-memory
    void initialize_indexes();

    void init_genesis(const genesis_state_type &genesis_state = genesis_state_type());

    template <typename EvaluatorType>
    void register_evaluator() //  注册验证模块
    {
        _operation_evaluators[operation::tag<typename EvaluatorType::operation_type>::value].reset(new op_evaluator_impl<EvaluatorType>());
    }

    //////////////////// db_balance.cpp ////////////////////

    /**
          * @brief Retrieve a particular account's balance in a given asset
          * @param owner Account whose balance should be retrieved
          * @param asset_id ID of the asset to get balance in
          * @return owner's balance in asset
          */
    asset get_balance(account_id_type owner, asset_id_type asset_id) const;
    /// This is an overloaded method.
    asset get_balance(const account_object &owner, const asset_object &asset_obj) const;

    /**
          * @brief Adjust a particular account's balance in a given asset by a delta
          * @param account ID of account whose balance should be adjusted
          * @param delta Asset ID and amount to adjust balance by
          */
    void adjust_balance(account_id_type account, asset delta,bool allow_gas_negative=false);

    /**
          * @brief Helper to make lazy deposit to CDD VBO.
          *
          * If the given optional VBID is not valid(),
          * or it does not have a CDD vesting policy,
          * or the owner / vesting_seconds of the policy
          * does not match the parameter, then credit amount
          * to newly created VBID and return it.
          *
          * Otherwise, credit amount to ovbid.
          * 
          * @return ID of newly created VBO, but only if VBO was created.
          */
    optional<vesting_balance_id_type> deposit_lazy_vesting(
        const optional<vesting_balance_id_type> &ovbid,
        asset amount,
        uint32_t req_vesting_seconds,
        account_id_type req_owner,
        string describe,
        bool require_vesting);

    // helper to handle cashback rewards
    void deposit_cashback(const account_object &acct, asset amount,string describe, bool require_vesting = true);
    // helper to handle witness pay
    void deposit_witness_pay(const witness_object &wit, asset amount);

    //////////////////// db_debug.cpp ////////////////////

    void debug_dump();
    void apply_debug_updates();
    void debug_update(const fc::variant_object &update);

    //////////////////// db_market.cpp ////////////////////

    /// @{ @group Market Helpers
    void globally_settle_asset(const asset_object &bitasset, const price &settle_price);
    void cancel_order(const force_settlement_object &order, bool create_virtual_op = true);
    void cancel_order(const limit_order_object &order, bool create_virtual_op = true);
    void revive_bitasset(const asset_object &bitasset);
    void cancel_bid(const collateral_bid_object &bid, bool create_virtual_op = true);
    void execute_bid(const collateral_bid_object &bid, share_type debt_covered, share_type collateral_from_fund, const price_feed &current_feed);

    /**
          * @brief Process a new limit order through the markets
          * @param order The new order to process
          * @return true if order was completely filled; false otherwise
          *
          * This function takes a new limit order, and runs the markets attempting to match it with existing orders
          * already on the books.
          */
    bool apply_order(const limit_order_object &new_order_object, bool allow_black_swan = true);

    /**
          * Matches the two orders,
          *
          * @return a bit field indicating which orders were filled (and thus removed)
          *
          * 0 - no orders were matched
          * 1 - bid was filled
          * 2 - ask was filled
          * 3 - both were filled
          */
    ///@{
    template <typename OrderType>
    int match(const limit_order_object &bid, const OrderType &ask, const price &match_price);
    int match(const limit_order_object &bid, const limit_order_object &ask, const price &trade_price);
    /// @return the amount of asset settled
    asset match(const call_order_object &call,
                const force_settlement_object &settle,
                const price &match_price,
                asset max_settlement);
    ///@}

    /**
          * @return true if the order was completely filled and thus freed.
          */
    bool fill_order(const limit_order_object &order, const asset &pays, const asset &receives, bool cull_if_small,
                    const price &fill_price, const bool is_maker);
    bool fill_order(const call_order_object &order, const asset &pays, const asset &receives,
                    const price &fill_price, const bool is_maker);
    bool fill_order(const force_settlement_object &settle, const asset &pays, const asset &receives,
                    const price &fill_price, const bool is_maker);

    bool check_call_orders(const asset_object &mia, bool enable_black_swan = true, bool for_new_limit_order = false);

    // helpers to fill_order
    void pay_order(const account_object &receiver, const asset &receives, const asset &pays);

    asset calculate_market_fee(const asset_object &recv_asset, const asset &trade_amount);
    asset pay_market_fees(const asset_object &recv_asset, const asset &receives);

    ///@}
    /**
          *  This method validates transactions without adding it to the pending state.
          *  @return true if the transaction would validate
          */
    processed_transaction validate_transaction(const signed_transaction &trx);

    /** when popping a block, the transactions that were removed get cached here so they
          * can be reapplied at the proper time */
    std::deque<signed_transaction> _popped_tx;
    uint16_t get_current_op_index()
    {
        return _current_op_in_trx;
    }
    /**
          * @}
          */
  protected:
    //Mark pop_undo() as protected -- we do not want outside calling pop_undo(); it should call pop_block() instead
    void pop_undo() { object_database::pop_undo(); }
    void notify_changed_objects();

  private:
    optional<undo_database::session> _pending_tx_session;
    vector<unique_ptr<op_evaluator>> _operation_evaluators;
    uint64_t _pending_size=0;
    map<account_id_type,bool> vote_result;
    template <typename ObjectType>
    vector<std::reference_wrapper<const ObjectType>> sort_votable_objects(vector<std::reference_wrapper<const ObjectType>> &refs,size_t count) const;


    //////////////////// db_block.cpp ////////////////////

  public:
    // these were formerly private, but they have a fairly well-defined API, so let's make them public
    void apply_block(const signed_block &next_block, uint32_t skip = skip_nothing);
    processed_transaction apply_transaction(const signed_transaction &trx, uint32_t skip = skip_nothing, transaction_apply_mode run_mode = transaction_apply_mode::apply_block_mode);
    operation_result apply_operation(transaction_evaluation_state &eval_state, const operation &op, bool is_agreed_task = false);

  private:
    void _apply_block(const signed_block &next_block);
    processed_transaction _apply_transaction(const signed_transaction &trx, transaction_apply_mode &run_mode,bool only_try_permissions=false);
    void _cancel_bids_and_revive_mpa(const asset_object &bitasset, const asset_bitasset_data_object &bad);

    ///Steps involved in applying a new block
    ///@{

    const witness_object &validate_block_header(uint32_t skip, const signed_block &next_block) const;
    const witness_object &_validate_block_header(const signed_block &next_block) const;
    void create_block_summary(const signed_block &next_block);

    //////////////////// db_update.cpp ////////////////////
    void update_global_dynamic_data(const signed_block &b);
    void update_signing_witness(const witness_object &signing_witness, const signed_block &new_block);
    void update_last_irreversible_block();
    void clear_expired_transactions();
    void clear_expired_nh_asset_orders();
    void clear_expired_proposals();
    void clear_expired_orders();
    void update_expired_feeds();
    void update_maintenance_flag(bool new_maintenance_flag);
    bool check_for_blackswan(const asset_object &mia, bool enable_black_swan = true);

    ///Steps performed only at maintenance intervals
    ///@{

    //////////////////// db_maint.cpp ////////////////////

    void initialize_budget_record(fc::time_point_sec now, budget_record &rec) const;
    void process_budget(const global_property_object old_gpo);
    void pay_workers(share_type &budget);
    void pay_candidates(share_type &budget,const uint16_t&committee_percent_of_candidate_award,const uint16_t&unsuccessful_candidates_percent);
    void perform_chain_maintenance(const signed_block &next_block, const global_property_object &global_props);
    void update_active_witnesses();
    void update_active_committee_members();
    void process_bids(const asset_bitasset_data_object &bad);

    template <class... Types>
    void perform_account_maintenance(std::tuple<Types...> helpers);
    ///@}
    ///@}

    vector<processed_transaction> _pending_tx;
    fork_database _fork_db;

    /**
          *  Note: we can probably store blocks by block num rather than
          *  block id because after the undo window is past the block ID
          *  is no longer relevant and its number is irreversible.
          *
          *  During the "fork window" we can cache blocks in memory
          *  until the fork is resolved.  This should make maintaining
          *  the fork tree relatively simple.
          */
    block_database _block_id_to_block;

    /**
          * Contains the set of ops that are in the process of being applied from
          * the current block.  It contains real and virtual operations in the
          * order they occur and is cleared after the applied_block signal is
          * emited.
          */
    vector<optional<operation_history_object>> _applied_ops;

    uint32_t _current_block_num = 0;
    uint16_t _current_trx_in_block = 0;
    uint16_t _current_op_in_trx = 0;
    uint16_t _current_virtual_op = 0;

    flat_map<uint32_t, block_id_type> _checkpoints;

    node_property_object _node_property_object;
    /*******************************nico add 线程锁*****************************/
    boost::recursive_mutex _db_lock;

    graphene::chain::lua_scheduler luaVM;
    public:
     const asset_object *core=nullptr;
     const asset_object *GAS=nullptr;
};

namespace detail
{
template <int... Is>
struct seq
{
};

template <int N, int... Is>
struct gen_seq : gen_seq<N - 1, N - 1, Is...>
{
};

template <int... Is>
struct gen_seq<0, Is...> : seq<Is...>
{
};

template <typename T, int... Is>
void for_each(T &&t, const account_object &a, seq<Is...>)
{
    auto l = {(std::get<Is>(t)(a), 0)...};
    (void)l;
}
} // namespace detail

} // namespace chain
} // namespace graphene
