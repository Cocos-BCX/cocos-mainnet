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
#include <cctype>

#include <graphene/app/api.hpp>
#include <graphene/app/api_access.hpp>
#include <graphene/app/application.hpp>
#include <graphene/app/impacted.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/get_config.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/transaction_object.hpp>
#include <graphene/chain/worker_object.hpp>

#include <fc/crypto/hex.hpp>
#include <fc/smart_ref_impl.hpp>
#include <fc/thread/future.hpp>

namespace graphene
{
namespace app
{

login_api::login_api(application &a)
    : _app(a)
{
}

login_api::~login_api()
{
}

bool login_api::login(const string &user, const string &password)
{
  optional<api_access_info> acc = _app.get_api_access_info(user);
  if (!acc.valid())
    return false;
  if (acc->password_hash_b64 != "*")
  {
    std::string password_salt = fc::base64_decode(acc->password_salt_b64);
    std::string acc_password_hash = fc::base64_decode(acc->password_hash_b64);

    fc::sha256 hash_obj = fc::sha256::hash(password + password_salt);
    if (hash_obj.data_size() != acc_password_hash.length())
      return false;
    if (memcmp(hash_obj.data(), acc_password_hash.c_str(), hash_obj.data_size()) != 0)
      return false;
    is_general_rights = false;
  }

  for (const std::string &api_name : acc->allowed_apis)
    enable_api(api_name); //enable the API named api_name, as the database_api will be enabled when it was successfully accessed, other APIs can also registered here
  return true;
}

void login_api::enable_api(const std::string &api_name)
{
  if (api_name == "database_api")
  {
    _database_api = std::make_shared<database_api>(std::ref(*_app.chain_database())); //using std::make_shared to safely allocate and use dynamic memory
  }
  else if (api_name == "block_api")
  {
    _block_api = std::make_shared<block_api>(std::ref(*_app.chain_database()));
  }
  else if (api_name == "network_broadcast_api")
  {
    _network_broadcast_api = std::make_shared<network_broadcast_api>(std::ref(_app));
  }
  else if (api_name == "history_api")
  {
    _history_api = std::make_shared<history_api>(this->_app);
  }
  else if (api_name == "network_node_api")
  {
    _network_node_api = std::make_shared<network_node_api>(std::ref(_app), !is_general_rights);
  }
 
  else if (api_name == "asset_api")
  {
    _asset_api = std::make_shared<asset_api>(std::ref(*_app.chain_database()));
  }
  else if (api_name == "debug_api")
  {
    // can only enable this API if the plugin was loaded
    if (_app.get_plugin("debug_witness"))
      _debug_api = std::make_shared<graphene::debug_witness::debug_api>(std::ref(_app));
  }
  return;
}

// block_api
block_api::block_api(graphene::chain::database &db) : _db(db) {}
block_api::~block_api() {}

vector<optional<signed_block>> block_api::get_blocks(uint32_t block_num_from, uint32_t block_num_to) const
{
  FC_ASSERT(block_num_to >= block_num_from);
  vector<optional<signed_block>> res;
  for (uint32_t block_num = block_num_from; block_num <= block_num_to; block_num++)
  {
    res.push_back(_db.fetch_block_by_number(block_num));
  }
  return res;
}

network_broadcast_api::network_broadcast_api(application &a) : _app(a)
{
  _applied_block_connection = _app.chain_database()->applied_block.connect([this](const signed_block &b) { on_applied_block(b); });
  _network_quality = _app.chain_database()->allowe_continue_transaction.connect([this](const uint32_t participating, bool maybe_allow_transaction) {
    this->participating = participating;
    this->maybe_allow_transaction = maybe_allow_transaction;
  });
  _rebroadcasts = _app.chain_database()->p2p_broadcast.connect([this](const signed_transaction tx) {
    //wlog("rebroadcasts tx:${tx}",("tx",tx));
    _app.p2p_node()->broadcast_transaction(tx);
  });
}

void network_broadcast_api::on_applied_block(const signed_block &b)
{
  if (_callbacks.size())
  {
    /// we need to ensure the database_api is not deleted for the life of the async operation
    auto capture_this = shared_from_this();
    for (uint32_t trx_num = 0; trx_num < b.transactions.size(); ++trx_num)
    {
      const auto trx_p = b.transactions[trx_num];
      const auto &trx = trx_p.second;
      auto id = trx_p.first;
      auto itr = _callbacks.find(id);
      if (itr != _callbacks.end())
      {
        auto block_num = b.block_num();
        auto &callback = itr->second;
        fc::async([capture_this, this, id, block_num, trx_num, trx, callback]() { callback(fc::variant(transaction_confirmation{id, block_num, trx_num, trx})); });
        _callbacks.erase(itr);
      }
    }
  }
}

tx_hash_type network_broadcast_api::broadcast_transaction(const signed_transaction &trx)
{
  FC_ASSERT(maybe_allow_transaction, "The current network quality is poor, and any transaction will be refused. \
      node appears to be on a minority fork with only ${pct}/10000 witness participation",
            ("pct", participating));
  _app.chain_database()->push_transaction(trx, 0, transaction_push_state::from_me);
  _app.p2p_node()->broadcast_transaction(trx);
  return trx.hash();
}

fc::variant network_broadcast_api::broadcast_transaction_synchronous(const signed_transaction &trx)
{
  fc::promise<fc::variant>::ptr prom(new fc::promise<fc::variant>());
  broadcast_transaction_with_callback([=](const fc::variant &v) {
    prom->set_value(v);
  },
                                      trx);

  return fc::future<fc::variant>(prom).wait();
}

void network_broadcast_api::broadcast_block(const signed_block &b)
{
  _app.chain_database()->push_block(b);
  _app.p2p_node()->broadcast(net::block_message(b));
}

void network_broadcast_api::broadcast_transaction_with_callback(confirmation_callback cb, const signed_transaction &trx)
{
  FC_ASSERT(maybe_allow_transaction, "The current network quality is poor, and any transaction will be refused. \
      node appears to be on a minority fork with only ${pct}/10000 witness participation",
            ("pct", participating));
  _app.chain_database()->push_transaction(trx, 0, transaction_push_state::from_me);
  auto hash = trx.hash();
  _callbacks[hash] = cb;
  _app.p2p_node()->broadcast_transaction(trx);
}

network_node_api::network_node_api(application &a, bool enable_set) : _app(a), enable_set(enable_set)
{
}

fc::variant_object network_node_api::get_info() const
{
  fc::mutable_variant_object result = _app.p2p_node()->network_get_info();
  result["connection_count"] = _app.p2p_node()->get_connection_count();
  return result;
}

void network_node_api::add_node(const fc::ip::endpoint &ep)
{
  FC_ASSERT(enable_set, "No permission to modify node link information");
  _app.p2p_node()->add_node(ep);
}

std::vector<net::peer_status> network_node_api::get_connected_peers() const
{
  return _app.p2p_node()->get_connected_peers();
}

std::vector<net::potential_peer_record> network_node_api::get_potential_peers() const
{
  return _app.p2p_node()->get_potential_peers();
}

fc::variant_object network_node_api::get_advanced_node_parameters() const
{
  return _app.p2p_node()->get_advanced_node_parameters();
}

void network_node_api::set_advanced_node_parameters(const fc::variant_object &params)
{
  FC_ASSERT(enable_set, "No permission to modify node link information");
  _app.p2p_node()->set_advanced_node_parameters(params);
}
void network_node_api::set_message_send_cache_size(const uint32_t &params)
{
  FC_ASSERT(enable_set, "No permission to set node message send cache size");
  _app.chain_database()->set_message_cache_size_limit(params);
}
void network_node_api::set_deduce_in_verification_mode(const bool &params)
{
  FC_ASSERT(enable_set, "No permission to set node deduce in verification mode");
  _app.chain_database()->set_deduce_in_verification_mode(params);
}

fc::api<network_broadcast_api> login_api::network_broadcast() const
{
  FC_ASSERT(_network_broadcast_api);
  return *_network_broadcast_api;
}

fc::api<block_api> login_api::block() const
{
  FC_ASSERT(_block_api);
  return *_block_api;
}

fc::api<network_node_api> login_api::network_node() const
{
  FC_ASSERT(_network_node_api);
  return *_network_node_api;
}

fc::api<database_api> login_api::database() const
{
  FC_ASSERT(_database_api);
  return *_database_api;
}

fc::api<history_api> login_api::history() const
{
  FC_ASSERT(_history_api);
  return *_history_api;
}


fc::api<asset_api> login_api::asset() const
{
  FC_ASSERT(_asset_api);
  return *_asset_api;
}

fc::api<graphene::debug_witness::debug_api> login_api::debug() const
{
  FC_ASSERT(_debug_api);
  return *_debug_api;
}

vector<order_history_object> history_api::get_fill_order_history(asset_id_type a, asset_id_type b, uint32_t limit) const
{
  FC_ASSERT(_app.chain_database());
  const auto &db = *_app.chain_database();
  if (a > b)
    std::swap(a, b);
  const auto &history_idx = db.get_index_type<graphene::market_history::history_index>().indices().get<by_key>();
  history_key hkey;
  hkey.base = a;
  hkey.quote = b;
  hkey.sequence = std::numeric_limits<int64_t>::min();

  uint32_t count = 0;
  auto itr = history_idx.lower_bound(hkey);
  vector<order_history_object> result;
  while (itr != history_idx.end() && count < limit)
  {
    if (itr->key.base != a || itr->key.quote != b)
      break;
    result.push_back(*itr);
    ++itr;
    ++count;
  }

  return result;
}

vector<operation_history_object> history_api::get_account_history(account_id_type account,
                                                                  operation_history_id_type stop,
                                                                  unsigned limit,
                                                                  operation_history_id_type start) const
{
  FC_ASSERT(_app.chain_database());
  const auto &db = *_app.chain_database();
  FC_ASSERT(limit <= 100);
  vector<operation_history_object> result;
  const auto &stats = account(db).statistics(db);
  if (stats.most_recent_op == account_transaction_history_id_type())
    return result;
  const account_transaction_history_object *node = &stats.most_recent_op(db);
  if (start == operation_history_id_type())
    start = node->operation_id;

  while (node && node->operation_id.instance > stop.instance && result.size() < limit)
  {
    if (node->operation_id.instance <= start.instance)
      result.push_back(node->operation_id(db));
    if (node->next == account_transaction_history_id_type())
      node = nullptr;
    else
      node = &node->next(db);
  }
  if (stop.instance == 0 && result.size() < limit)
  {
    node = db.find(account_transaction_history_id_type());
    if (node && node->account == account)
      result.push_back(node->operation_id(db));
  }
  return result;
}

vector<operation_history_object> history_api::get_account_history_operations(account_id_type account,
                                                                             unsigned operation_id,
                                                                             operation_history_id_type start,
                                                                             operation_history_id_type stop,
                                                                             unsigned limit) const
{
  FC_ASSERT(_app.chain_database());
  const auto &db = *_app.chain_database();
  FC_ASSERT(limit <= 100);
  vector<operation_history_object> result;
  const auto &stats = account(db).statistics(db);
  if (stats.most_recent_op == account_transaction_history_id_type())
    return result;
  const account_transaction_history_object *node = &stats.most_recent_op(db);
  if (start == operation_history_id_type())
    start = node->operation_id;

  while (node && node->operation_id.instance > stop.instance && result.size() < limit)
  {
    if (node->operation_id.instance <= start.instance)
    {

      if (node->operation_id(db).op.which() == operation_id)
        result.push_back(node->operation_id(db));
    }
    if (node->next == account_transaction_history_id_type())
      node = nullptr;
    else
      node = &node->next(db);
  }
  if (stop.instance == 0 && result.size() < limit)
  {
    const account_transaction_history_object head = account_transaction_history_id_type()(db);
    if (head.account == account && head.operation_id(db).op.which() == operation_id)
      result.push_back(head.operation_id(db));
  }
  return result;
}

vector<operation_history_object> history_api::get_relative_account_history(account_id_type account,
                                                                           uint32_t stop,
                                                                           unsigned limit,
                                                                           uint32_t start) const
{
  FC_ASSERT(_app.chain_database());
  const auto &db = *_app.chain_database();
  FC_ASSERT(limit <= 100);
  vector<operation_history_object> result;
  const auto &stats = account(db).statistics(db);
  if (start == 0)
    start = stats.total_ops;
  else
    start = std::min(stats.total_ops, start);

  if (start >= stop && start > stats.removed_ops && limit > 0)
  {
    const auto &hist_idx = db.get_index_type<account_transaction_history_index>();
    const auto &by_seq_idx = hist_idx.indices().get<by_seq>();

    auto itr = by_seq_idx.upper_bound(boost::make_tuple(account, start));
    auto itr_stop = by_seq_idx.lower_bound(boost::make_tuple(account, stop));

    do
    {
      --itr;
      result.push_back(itr->operation_id(db));
    } while (itr != itr_stop && result.size() < limit);
  }
  return result;
}

flat_set<uint32_t> history_api::get_market_history_buckets() const
{
  auto hist = _app.get_plugin<market_history_plugin>("market_history");
  FC_ASSERT(hist);
  return hist->tracked_buckets();
}

vector<bucket_object> history_api::get_market_history(asset_id_type a, asset_id_type b,
                                                      uint32_t bucket_seconds, fc::time_point_sec start, fc::time_point_sec end) const
{
  try
  {
    FC_ASSERT(_app.chain_database());
    const auto &db = *_app.chain_database();
    vector<bucket_object> result;
    result.reserve(200);

    if (a > b)
      std::swap(a, b);

    const auto &bidx = db.get_index_type<bucket_index>();
    const auto &by_key_idx = bidx.indices().get<by_key>();

    auto itr = by_key_idx.lower_bound(bucket_key(a, b, bucket_seconds, start));
    while (itr != by_key_idx.end() && itr->key.open <= end && result.size() < 200)
    {
      if (!(itr->key.base == a && itr->key.quote == b && itr->key.seconds == bucket_seconds))
      {
        return result;
      }
      result.push_back(*itr);
      ++itr;
    }
    return result;
  }
  FC_CAPTURE_AND_RETHROW((a)(b)(bucket_seconds)(start)(end))
}

// asset_api
asset_api::asset_api(graphene::chain::database &db) : _db(db) {}
asset_api::~asset_api() {}

vector<account_asset_balance> asset_api::get_asset_holders(asset_id_type asset_id, uint32_t start, uint32_t limit) const
{
  FC_ASSERT(limit <= 100);

  const auto &bal_idx = _db.get_index_type<account_balance_index>().indices().get<by_asset_balance>();
  auto range = bal_idx.equal_range(boost::make_tuple(asset_id));

  vector<account_asset_balance> result;

  uint32_t index = 0;
  for (const account_balance_object &bal : boost::make_iterator_range(range.first, range.second))
  {
    if (result.size() >= limit)
      break;

    if (bal.balance == 0)
      continue;

    if (index++ < start)
      continue;

    const auto account = _db.find(bal.owner);

    account_asset_balance aab;
    aab.name = account->name;
    aab.account_id = account->id;
    aab.amount = bal.balance;

    result.push_back(aab);
  }

  return result;
}
// get number of asset holders.
int asset_api::get_asset_holders_count(asset_id_type asset_id) const
{

  const auto &bal_idx = _db.get_index_type<account_balance_index>().indices().get<by_asset_balance>();
  auto range = bal_idx.equal_range(boost::make_tuple(asset_id));

  int count = boost::distance(range) - 1;

  return count;
}
// function to get vector of system assets with holders count.
vector<asset_holders> asset_api::get_all_asset_holders() const
{

  vector<asset_holders> result;

  vector<asset_id_type> total_assets;
  for (const asset_object &asset_obj : _db.get_index_type<asset_index>().indices())
  {
    const auto &dasset_obj = asset_obj.dynamic_asset_data_id(_db);

    asset_id_type asset_id;
    asset_id = dasset_obj.id;

    const auto &bal_idx = _db.get_index_type<account_balance_index>().indices().get<by_asset_balance>();
    auto range = bal_idx.equal_range(boost::make_tuple(asset_id));

    int count = boost::distance(range) - 1;

    asset_holders ah;
    ah.asset_id = asset_id;
    ah.count = count;

    result.push_back(ah);
  }

  return result;
}

} // namespace app
} // namespace graphene
