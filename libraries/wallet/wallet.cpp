/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <list>

#include <boost/version.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>

#include <boost/range/adaptor/map.hpp>
#include <boost/range/algorithm_ext/erase.hpp>
#include <boost/range/algorithm/unique.hpp>
#include <boost/range/algorithm/sort.hpp>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/hashed_index.hpp>

#include <fc/git_revision.hpp>
#include <fc/io/fstream.hpp>
#include <fc/io/json.hpp>
#include <fc/io/stdio.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/rpc/cli.hpp>
#include <fc/rpc/websocket_api.hpp>
#include <fc/crypto/aes.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/thread/mutex.hpp>
#include <fc/thread/scoped_lock.hpp>

#include <graphene/app/api.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/utilities/git_revision.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/utilities/words.hpp>
#include <graphene/wallet/wallet.hpp>
#include <graphene/wallet/api_documentation.hpp>
#include <graphene/wallet/reflect_util.hpp>
#include <graphene/debug_witness/debug_api.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/chain/protocol/nh_asset.hpp>

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#endif

#define BRAIN_KEY_WORD_COUNT 16

namespace graphene
{
namespace wallet
{

namespace detail
{

struct operation_result_printer
{
public:
      operation_result_printer(const wallet_api_impl &w)
          : _wallet(w) {}
      const wallet_api_impl &_wallet;
      typedef std::string result_type;

      std::string operator()(const void_result &x) const;
      std::string operator()(const object_id_result &oid);
      std::string operator()(const asset_result &a);
      std::string operator()(const contract_result &a);
      std::string operator()(const logger_result &err);
      std::string operator()(const error_result &err);
};

// BLOCK  TRX  OP  VOP
struct operation_printer
{
private:
      ostream &out;
      const wallet_api_impl &wallet;
      operation_result result;

      std::string fee(const asset &a) const;

public:
      operation_printer(ostream &out, const wallet_api_impl &wallet, const operation_result &r = operation_result())
          : out(out),
            wallet(wallet),
            result(r)
      {
      }
      typedef std::string result_type;

      template <typename T>
      std::string operator()(const T &op) const;

      std::string operator()(const account_create_operation &op) const;
      std::string operator()(const account_update_operation &op) const;
      std::string operator()(const asset_create_operation &op) const;
};

template <class T>
fc::optional<T> maybe_id(const string &name_or_id)
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
      return fc::optional<T>();
}

string address_to_shorthash(const address &addr)
{
      uint32_t x = addr.addr._hash[0];
      static const char hd[] = "0123456789abcdef";
      string result;

      result += hd[(x >> 0x1c) & 0x0f];
      result += hd[(x >> 0x18) & 0x0f];
      result += hd[(x >> 0x14) & 0x0f];
      result += hd[(x >> 0x10) & 0x0f];
      result += hd[(x >> 0x0c) & 0x0f];
      result += hd[(x >> 0x08) & 0x0f];
      result += hd[(x >> 0x04) & 0x0f];
      result += hd[(x)&0x0f];

      return result;
}

fc::ecc::private_key derive_private_key(const std::string &prefix_string,
                                        int sequence_number)
{
      std::string sequence_string = std::to_string(sequence_number);
      fc::sha512 h = fc::sha512::hash(prefix_string + " " + sequence_string);
      fc::ecc::private_key derived_key = fc::ecc::private_key::regenerate(fc::sha256::hash(h));
      return derived_key;
}

string normalize_brain_key(string s)
{
      size_t i = 0, n = s.length();
      std::string result;
      char c;
      result.reserve(n);

      bool preceded_by_whitespace = false;
      bool non_empty = false;
      while (i < n)
      {
            c = s[i++];
            switch (c)
            {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
            case '\v':
            case '\f':
                  preceded_by_whitespace = true;
                  continue;

            case 'a':
                  c = 'A';
                  break;
            case 'b':
                  c = 'B';
                  break;
            case 'c':
                  c = 'C';
                  break;
            case 'd':
                  c = 'D';
                  break;
            case 'e':
                  c = 'E';
                  break;
            case 'f':
                  c = 'F';
                  break;
            case 'g':
                  c = 'G';
                  break;
            case 'h':
                  c = 'H';
                  break;
            case 'i':
                  c = 'I';
                  break;
            case 'j':
                  c = 'J';
                  break;
            case 'k':
                  c = 'K';
                  break;
            case 'l':
                  c = 'L';
                  break;
            case 'm':
                  c = 'M';
                  break;
            case 'n':
                  c = 'N';
                  break;
            case 'o':
                  c = 'O';
                  break;
            case 'p':
                  c = 'P';
                  break;
            case 'q':
                  c = 'Q';
                  break;
            case 'r':
                  c = 'R';
                  break;
            case 's':
                  c = 'S';
                  break;
            case 't':
                  c = 'T';
                  break;
            case 'u':
                  c = 'U';
                  break;
            case 'v':
                  c = 'V';
                  break;
            case 'w':
                  c = 'W';
                  break;
            case 'x':
                  c = 'X';
                  break;
            case 'y':
                  c = 'Y';
                  break;
            case 'z':
                  c = 'Z';
                  break;

            default:
                  break;
            }
            if (preceded_by_whitespace && non_empty)
                  result.push_back(' ');
            result.push_back(c);
            preceded_by_whitespace = false;
            non_empty = true;
      }
      return result;
}

struct op_prototype_visitor
{
      typedef void result_type;

      int t = 0;
      flat_map<std::string, operation> &name2op;

      op_prototype_visitor(
          int _t,
          flat_map<std::string, operation> &_prototype_ops) : t(_t), name2op(_prototype_ops) {}

      template <typename Type>
      result_type operator()(const Type &op) const
      {
            string name = fc::get_typename<Type>::name();
            size_t p = name.rfind(':');
            if (p != string::npos)
                  name = name.substr(p + 1);
            name2op[name] = Type();
      }
};

class wallet_api_impl
{
public:
      api_documentation method_documentation;

private:
      void claim_registered_account(const account_object &account)
      {
            auto it = _wallet.pending_account_registrations.find(account.name);
            FC_ASSERT(it != _wallet.pending_account_registrations.end());
            for (const std::string &wif_key : it->second)
                  if (!import_key(account.name, wif_key))
                  {
                        // somebody else beat our pending registration, there is
                        //   nothing we can do except log it and move on
                        elog("account ${name} registered by someone else first!",
                             ("name", account.name));
                        // might as well remove it from pending regs,
                        //   because there is now no way this registration
                        //   can become valid (even in the extremely rare
                        //   possibility of migrating to a fork where the
                        //   name is available, the user can always
                        //   manually re-register)
                  }
            _wallet.pending_account_registrations.erase(it);
      }

      // after a witness registration succeeds, this saves the private key in the wallet permanently
      //
      void claim_registered_witness(const std::string &witness_name)
      {
            auto iter = _wallet.pending_witness_registrations.find(witness_name);
            FC_ASSERT(iter != _wallet.pending_witness_registrations.end());
            std::string wif_key = iter->second;

            // get the list key id this key is registered with in the chain
            fc::optional<fc::ecc::private_key> witness_private_key = wif_to_key(wif_key);
            FC_ASSERT(witness_private_key);

            auto pub_key = witness_private_key->get_public_key();
            _keys[pub_key] = wif_key;
            _wallet.pending_witness_registrations.erase(iter);
      }

      fc::mutex _resync_mutex;
      void resync()
      {
            fc::scoped_lock<fc::mutex> lock(_resync_mutex);
            // this method is used to update wallet_data annotations
            //  e.g. wallet has been restarted and was not notified
            //  of events while it was down
            //
            // everything that is done "incremental style" when a push
            //  notification is received, should also be done here
            //  "batch style" by querying the blockchain

            if (!_wallet.pending_account_registrations.empty())
            {
                  // make a vector of the account names pending registration
                  std::vector<string> pending_account_names = boost::copy_range<std::vector<string>>(boost::adaptors::keys(_wallet.pending_account_registrations));

                  // look those up on the blockchain
                  std::vector<fc::optional<graphene::chain::account_object>>
                      pending_account_objects = _remote_db->lookup_account_names(pending_account_names);

                  // if any of them exist, claim them
                  for (const fc::optional<graphene::chain::account_object> &optional_account : pending_account_objects)
                        if (optional_account)
                              claim_registered_account(*optional_account);
            }

            if (!_wallet.pending_witness_registrations.empty())
            {
                  // make a vector of the owner accounts for witnesses pending registration
                  std::vector<string> pending_witness_names = boost::copy_range<std::vector<string>>(boost::adaptors::keys(_wallet.pending_witness_registrations));

                  // look up the owners on the blockchain
                  std::vector<fc::optional<graphene::chain::account_object>> owner_account_objects = _remote_db->lookup_account_names(pending_witness_names);

                  // if any of them have registered witnesses, claim them
                  for (const fc::optional<graphene::chain::account_object> &optional_account : owner_account_objects)
                        if (optional_account)
                        {
                              fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(optional_account->id);
                              if (witness_obj)
                                    claim_registered_witness(optional_account->name);
                        }
            }
      }

      void enable_umask_protection()
      {
#ifdef __unix__
            _old_umask = umask(S_IRWXG | S_IRWXO);
#endif
      }

      void disable_umask_protection()
      {
#ifdef __unix__
            umask(_old_umask);
#endif
      }

      void init_prototype_ops()
      {
            operation op;
            for (int t = 0; t < op.count(); t++)
            {
                  op.set_which(t);
                  op.visit(op_prototype_visitor(t, _prototype_ops));
            }
            return;
      }

      map<transaction_handle_type, signed_transaction> _builder_transactions;

      // if the user executes the same command twice in quick succession,
      // we might generate the same transaction id, and cause the second
      // transaction to be rejected.  This can be avoided by altering the
      // second transaction slightly (bumping up the expiration time by
      // a second).  Keep track of recent transaction ids we've generated
      // so we can know if we need to do this
      struct recently_generated_transaction_record
      {
            fc::time_point_sec generation_time;
            graphene::chain::transaction_id_type transaction_id;
      };
      struct timestamp_index
      {
      };
      typedef boost::multi_index_container<recently_generated_transaction_record,
                                           boost::multi_index::indexed_by<boost::multi_index::hashed_unique<
                                                                              boost::multi_index::member<recently_generated_transaction_record, graphene::chain::transaction_id_type,
                                                                                                         &recently_generated_transaction_record::transaction_id>,
                                                                              std::hash<graphene::chain::transaction_id_type>>,
                                                                          boost::multi_index::ordered_non_unique<boost::multi_index::tag<timestamp_index>,
                                                                                                                 boost::multi_index::member<recently_generated_transaction_record, fc::time_point_sec,
                                                                                                                                            &recently_generated_transaction_record::generation_time>>>>
          recently_generated_transaction_set_type;

      recently_generated_transaction_set_type _recently_generated_transactions;

      vector<private_key_type> extern_private_keys;

public:
      wallet_api &self;
      wallet_api_impl(wallet_api &s, const wallet_data &initial_data, fc::api<login_api> rapi)
          : self(s),
            _chain_id(initial_data.chain_id),
            _remote_api(rapi),
            _remote_db(rapi->database()),
            _remote_net_broadcast(rapi->network_broadcast()),
            _remote_hist(rapi->history())
      {
            chain_id_type remote_chain_id = _remote_db->get_chain_id();
            if (remote_chain_id != _chain_id)
            {
                  FC_THROW("Remote server gave us an unexpected chain_id",
                           ("remote_chain_id", remote_chain_id)("chain_id", _chain_id));
            }
            init_prototype_ops();

            _remote_db->set_block_applied_callback([this](const variant &block_id) {
                  on_block_applied(block_id);
            });

            _wallet.chain_id = _chain_id;
            _wallet.ws_server = initial_data.ws_server;
            _wallet.ws_user = initial_data.ws_user;
            _wallet.ws_password = initial_data.ws_password;
      }
      int add_extern_sign_key(string wif_key)
      {
            fc::optional<private_key_type> temp_key = wif_to_key(wif_key);
            if (temp_key)
                  extern_private_keys.push_back(*temp_key);
            return extern_private_keys.size();
      };
      virtual ~wallet_api_impl()
      {
            try
            {
                  _remote_db->cancel_all_subscriptions();
            }
            catch (const fc::exception &e)
            {
                  // Right now the wallet_api has no way of knowing if the connection to the
                  // witness has already disconnected (via the witness node exiting first).
                  // If it has exited, cancel_all_subscriptsions() will throw and there's
                  // nothing we can do about it.
                  // dlog("Caught exception ${e} while canceling database subscriptions", ("e", e));
            }
      }

      void encrypt_keys()
      {
            if (!is_locked())
            {
                  plain_keys data;
                  data.keys = _keys;
                  data.checksum = _checksum;
                  auto plain_txt = fc::raw::pack(data);
                  _wallet.cipher_keys = fc::aes_encrypt(data.checksum, plain_txt);
            }
      }

      void on_block_applied(const variant &block_id)
      {
            fc::async([this] { resync(); }, "Resync after block");
      }

      bool copy_wallet_file(string destination_filename)
      {
            fc::path src_path = get_wallet_filename();
            if (!fc::exists(src_path))
                  return false;
            fc::path dest_path = destination_filename + _wallet_filename_extension;
            int suffix = 0;
            while (fc::exists(dest_path))
            {
                  ++suffix;
                  dest_path = destination_filename + "-" + to_string(suffix) + _wallet_filename_extension;
            }
            wlog("backing up wallet ${src} to ${dest}",
                 ("src", src_path)("dest", dest_path));

            fc::path dest_parent = fc::absolute(dest_path).parent_path();
            try
            {
                  enable_umask_protection();
                  if (!fc::exists(dest_parent))
                        fc::create_directories(dest_parent);
                  fc::copy(src_path, dest_path);
                  disable_umask_protection();
            }
            catch (...)
            {
                  disable_umask_protection();
                  throw;
            }
            return true;
      }

      bool is_locked() const
      {
            return _checksum == fc::sha512();
      }

      template <typename T>
      T get_object(object_id<T::space_id, T::type_id, T> id) const
      {
            auto ob = _remote_db->get_objects({id}).front();
            return ob.template as<T>();
      }

      variant info() const
      {
            auto chain_props = get_chain_properties();
            auto global_props = get_global_properties();
            auto dynamic_props = get_dynamic_global_properties();
            fc::mutable_variant_object result;
            result["head_block_num"] = dynamic_props.head_block_number;
            result["head_block_id"] = dynamic_props.head_block_id;
            result["head_block_age"] = fc::get_approximate_relative_time_string(dynamic_props.time,
                                                                                time_point_sec(time_point::now()));
            result["next_maintenance_time"] = fc::get_approximate_relative_time_string(dynamic_props.next_maintenance_time);
            result["chain_id"] = chain_props.chain_id;
            result["participation"] = (100 * dynamic_props.recent_slots_filled.popcount()) / 128.0;
            result["active_witnesses"] = global_props.active_witnesses;
            result["active_committee_members"] = global_props.active_committee_members;
            return result;
      }

      variant_object about() const
      {
            string client_version(graphene::utilities::git_revision_description);
            const size_t pos = client_version.find('/');
            if (pos != string::npos && client_version.size() > pos)
                  client_version = client_version.substr(pos + 1);

            fc::mutable_variant_object result;
            //result["blockchain_name"]        = BLOCKCHAIN_NAME;
            //result["blockchain_description"] = BTS_BLOCKCHAIN_DESCRIPTION;
            result["client_version"] = client_version;
            result["graphene_revision"] = graphene::utilities::git_revision_sha;
            result["graphene_revision_age"] = fc::get_approximate_relative_time_string(fc::time_point_sec(graphene::utilities::git_revision_unix_timestamp));
            result["fc_revision"] = fc::git_revision_sha;
            result["fc_revision_age"] = fc::get_approximate_relative_time_string(fc::time_point_sec(fc::git_revision_unix_timestamp));
            result["compile_date"] = "compiled on " __DATE__ " at " __TIME__;
            result["boost_version"] = boost::replace_all_copy(std::string(BOOST_LIB_VERSION), "_", ".");
            result["openssl_version"] = OPENSSL_VERSION_TEXT;

            std::string bitness = boost::lexical_cast<std::string>(8 * sizeof(int *)) + "-bit";
#if defined(__APPLE__)
            std::string os = "osx";
#elif defined(__linux__)
            std::string os = "linux";
#elif defined(_MSC_VER)
            std::string os = "win32";
#else
            std::string os = "other";
#endif
            result["build"] = os + " " + bitness;

            return result;
      }

      chain_property_object get_chain_properties() const
      {
            return _remote_db->get_chain_properties();
      }
      global_property_object get_global_properties() const
      {
            return _remote_db->get_global_properties();
      }
      dynamic_global_property_object get_dynamic_global_properties() const
      {
            return _remote_db->get_dynamic_global_properties();
      }
      account_object get_account(account_id_type id) const
      {
            if (_wallet.my_accounts.get<by_id>().count(id))
                  return *_wallet.my_accounts.get<by_id>().find(id);
            auto rec = _remote_db->get_accounts({id}).front();
            FC_ASSERT(rec);
            return *rec;
      }
      account_object get_account(string account_name_or_id) const
      {
            FC_ASSERT(account_name_or_id.size() > 0);

            if (auto id = maybe_id<account_id_type>(account_name_or_id))
            {
                  // It's an ID
                  return get_account(*id);
            }
            else
            {
                  // It's a name
                  if (_wallet.my_accounts.get<by_name>().count(account_name_or_id))
                  {
                        auto local_account = *_wallet.my_accounts.get<by_name>().find(account_name_or_id);
                        auto blockchain_account = _remote_db->lookup_account_names({account_name_or_id}).front();
                        FC_ASSERT(blockchain_account);
                        if (local_account.id != blockchain_account->id)
                              elog("my account id ${id} different from blockchain id ${id2}", ("id", local_account.id)("id2", blockchain_account->id));
                        if (local_account.name != blockchain_account->name)
                              elog("my account name ${id} different from blockchain name ${id2}", ("id", local_account.name)("id2", blockchain_account->name));

                        return *_wallet.my_accounts.get<by_name>().find(account_name_or_id);
                  }
                  auto rec = _remote_db->lookup_account_names({account_name_or_id}).front();
                  FC_ASSERT(rec && rec->name == account_name_or_id);
                  return *rec;
            }
      }
      account_id_type get_account_id(string account_name_or_id) const
      {
            return get_account(account_name_or_id).get_id();
      }
      fc::optional<asset_object> find_asset(asset_id_type id) const
      {
            auto rec = _remote_db->get_assets({id}).front();
            if (rec)
                  _asset_cache[id] = *rec;
            return rec;
      }
      fc::optional<asset_object> find_asset(string asset_symbol_or_id) const
      {
            FC_ASSERT(asset_symbol_or_id.size() > 0);

            if (auto id = maybe_id<asset_id_type>(asset_symbol_or_id))
            {
                  // It's an ID
                  return find_asset(*id);
            }
            else
            {
                  // It's a symbol
                  auto rec = _remote_db->lookup_asset_symbols({asset_symbol_or_id}).front();
                  if (rec)
                  {
                        if (rec->symbol != asset_symbol_or_id)
                              return fc::optional<asset_object>();

                        _asset_cache[rec->get_id()] = *rec;
                  }
                  return rec;
            }
      }
      asset_object get_asset(asset_id_type id) const
      {
            auto opt = find_asset(id);
            FC_ASSERT(opt);
            return *opt;
      }
      asset_object get_asset(string asset_symbol_or_id) const
      {
            auto opt = find_asset(asset_symbol_or_id);
            FC_ASSERT(opt);
            return *opt;
      }

      asset_id_type get_asset_id(string asset_symbol_or_id) const
      {
            FC_ASSERT(asset_symbol_or_id.size() > 0);
            vector<fc::optional<asset_object>> opt_asset;
            if (std::isdigit(asset_symbol_or_id.front()))
                  return fc::variant(asset_symbol_or_id).as<asset_id_type>();
            opt_asset = _remote_db->lookup_asset_symbols({asset_symbol_or_id});
            FC_ASSERT((opt_asset.size() > 0) && (opt_asset[0].valid()));
            return opt_asset[0]->id;
      }

      string get_wallet_filename() const
      {
            return _wallet_filename;
      }

      fc::ecc::private_key get_private_key(const public_key_type &id) const
      {
            auto it = _keys.find(id);
            FC_ASSERT(it != _keys.end());

            fc::optional<fc::ecc::private_key> privkey = wif_to_key(it->second);
            FC_ASSERT(privkey);
            return *privkey;
      }

      fc::ecc::private_key get_private_key_for_account(const account_object &account) const
      {
            vector<public_key_type> active_keys = account.active.get_keys();
            if (active_keys.size() != 1)
                  FC_THROW("Expecting a simple authority with one active key");
            return get_private_key(active_keys.front());
      }

      // imports the private key into the wallet, and associate it in some way (?) with the
      // given account name.
      // @returns true if the key matches a current active/owner/memo key for the named
      //         account, false otherwise (but it is stored either way)
      bool import_key(string account_name_or_id, string wif_key)
      {
            fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
            if (!optional_private_key)
                  FC_THROW("Invalid private key");
            graphene::chain::public_key_type wif_pub_key = optional_private_key->get_public_key();

            account_object account = get_account(account_name_or_id);

            // make a list of all current public keys for the named account
            flat_set<public_key_type> all_keys_for_account;
            std::vector<public_key_type> active_keys = account.active.get_keys();
            std::vector<public_key_type> owner_keys = account.owner.get_keys();
            std::copy(active_keys.begin(), active_keys.end(), std::inserter(all_keys_for_account, all_keys_for_account.end()));
            std::copy(owner_keys.begin(), owner_keys.end(), std::inserter(all_keys_for_account, all_keys_for_account.end()));
            all_keys_for_account.insert(account.options.memo_key);
            //account.options.extensions.insert("string,int,void_t");
            _keys[wif_pub_key] = wif_key;

            _wallet.update_account(account);

            _wallet.extra_keys[account.id].insert(wif_pub_key);

            return all_keys_for_account.find(wif_pub_key) != all_keys_for_account.end();
      }

      vector<signed_transaction> import_balance(string name_or_id, const vector<string> &wif_keys, bool broadcast);

      bool load_wallet_file(string wallet_filename = "")
      {
            // TODO:  Merge imported wallet with existing wallet,
            //       instead of replacing it
            if (wallet_filename == "")
                  wallet_filename = _wallet_filename;

            if (!fc::exists(wallet_filename))
                  return false;

            _wallet = fc::json::from_file(wallet_filename).as<wallet_data>();
            if (_wallet.chain_id != _chain_id)
                  FC_THROW("Wallet chain ID does not match",
                           ("wallet.chain_id", _wallet.chain_id)("chain_id", _chain_id));

            size_t account_pagination = 100;
            vector<account_id_type> account_ids_to_send;
            size_t n = _wallet.my_accounts.size();
            account_ids_to_send.reserve(std::min(account_pagination, n));
            auto it = _wallet.my_accounts.begin();

            for (size_t start = 0; start < n; start += account_pagination)
            {
                  size_t end = std::min(start + account_pagination, n);
                  assert(end > start);
                  account_ids_to_send.clear();
                  std::vector<account_object> old_accounts;
                  for (size_t i = start; i < end; i++)
                  {
                        assert(it != _wallet.my_accounts.end());
                        old_accounts.push_back(*it);
                        account_ids_to_send.push_back(old_accounts.back().id);
                        ++it;
                  }
                  std::vector<fc::optional<account_object>> accounts = _remote_db->get_accounts(account_ids_to_send);
                  // server response should be same length as request
                  FC_ASSERT(accounts.size() == account_ids_to_send.size());
                  size_t i = 0;
                  for (const fc::optional<account_object> &acct : accounts)
                  {
                        account_object &old_acct = old_accounts[i];
                        if (!acct.valid())
                        {
                              elog("Could not find account ${id} : \"${name}\" does not exist on the chain!", ("id", old_acct.id)("name", old_acct.name));
                              i++;
                              continue;
                        }
                        // this check makes sure the server didn't send results
                        // in a different order, or accounts we didn't request
                        FC_ASSERT(acct->id == old_acct.id);
                        if (fc::json::to_string(*acct) != fc::json::to_string(old_acct))
                        {
                              wlog("Account ${id} : \"${name}\" updated on chain", ("id", acct->id)("name", acct->name));
                        }
                        _wallet.update_account(*acct);
                        i++;
                  }
            }

            return true;
      }
      void save_wallet_file(string wallet_filename = "")
      {
            //
            // Serialize in memory, then save to disk
            //
            // This approach lessens the risk of a partially written wallet
            // if exceptions are thrown in serialization
            //

            encrypt_keys();

            if (wallet_filename == "")
                  wallet_filename = _wallet_filename;

            wlog("saving wallet to file ${fn}", ("fn", wallet_filename));

            string data = fc::json::to_pretty_string(_wallet);
            try
            {
                  enable_umask_protection();
                  //
                  // Parentheses on the following declaration fails to compile,
                  // due to the Most Vexing Parse.  Thanks, C++
                  //
                  // http://en.wikipedia.org/wiki/Most_vexing_parse
                  //
                  fc::ofstream outfile{fc::path(wallet_filename)};
                  outfile.write(data.c_str(), data.length());
                  outfile.flush();
                  outfile.close();
                  disable_umask_protection();
            }
            catch (...)
            {
                  disable_umask_protection();
                  throw;
            }
      }

      transaction_handle_type begin_builder_transaction()
      {
            int trx_handle = _builder_transactions.empty() ? 0
                                                           : (--_builder_transactions.end())->first + 1;
            _builder_transactions[trx_handle];
            return trx_handle;
      }
      void add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation &op)
      {
            FC_ASSERT(_builder_transactions.count(transaction_handle));
            _builder_transactions[transaction_handle].operations.emplace_back(op);
      }
      void replace_operation_in_builder_transaction(transaction_handle_type handle,
                                                    uint32_t operation_index,
                                                    const operation &new_op)
      {
            FC_ASSERT(_builder_transactions.count(handle));
            signed_transaction &trx = _builder_transactions[handle];
            FC_ASSERT(operation_index < trx.operations.size());
            trx.operations[operation_index] = new_op;
      }
      transaction preview_builder_transaction(transaction_handle_type handle)
      {
            FC_ASSERT(_builder_transactions.count(handle));
            return _builder_transactions[handle];
      }
      signed_transaction sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast = true)
      {
            FC_ASSERT(_builder_transactions.count(transaction_handle));
            return _builder_transactions[transaction_handle] = sign_transaction(_builder_transactions[transaction_handle], broadcast);
      }

      signed_transaction propose_builder_transaction(
          transaction_handle_type handle,
          string account_name_or_id,
          time_point_sec expiration = time_point::now() + fc::minutes(1),
          uint32_t review_period_seconds = 0, bool broadcast = true)
      {
            FC_ASSERT(_builder_transactions.count(handle));
            proposal_create_operation op;
            op.fee_paying_account = get_account(account_name_or_id).get_id();
            op.expiration_time = expiration;
            signed_transaction trx = _builder_transactions[handle];
            std::transform(trx.operations.begin(), trx.operations.end(), std::back_inserter(op.proposed_ops),
                           [](const operation &op) -> op_wrapper { return op; });
            if (review_period_seconds)
                  op.review_period_seconds = review_period_seconds;
            trx.operations = {op};

            return trx = sign_transaction(trx, broadcast);
      }

      void remove_builder_transaction(transaction_handle_type handle)
      {
            _builder_transactions.erase(handle);
      }

      signed_transaction register_account(string name,
                                          public_key_type owner,
                                          public_key_type active,
                                          string registrar_account,
                                          bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!self.is_locked());
                  FC_ASSERT(is_valid_name(name));
                  account_create_operation account_create_op;

                  // TODO:  process when pay_from_account is ID

                  account_object registrar_account_object =
                      this->get_account(registrar_account);
                  FC_ASSERT(registrar_account_object.is_lifetime_member());

                  account_id_type registrar_account_id = registrar_account_object.id;

                  account_create_op.registrar = registrar_account_id;
                  account_create_op.name = name;
                  account_create_op.owner = authority(1, owner, 1);
                  account_create_op.active = authority(1, active, 1);
                  account_create_op.options.memo_key = active;
                  signed_transaction tx;

                  tx.operations.push_back(account_create_op);

                  auto current_fees = _remote_db->get_global_properties().parameters.current_fees;
                  vector<public_key_type> paying_keys = registrar_account_object.active.get_keys();

                  auto dyn_props = get_dynamic_global_properties();
                  tx.set_reference_block(dyn_props.head_block_id);
                  tx.set_expiration(dyn_props.time + fc::seconds(30));
                  tx.validate();

                  for (public_key_type &key : paying_keys)
                  {
                        auto it = _keys.find(key);
                        if (it != _keys.end())
                        {
                              fc::optional<fc::ecc::private_key> privkey = wif_to_key(it->second);
                              if (!privkey.valid())
                              {
                                    FC_ASSERT(false, "Malformed private key in _keys");
                              }
                              tx.sign(*privkey, _chain_id);
                        }
                  }

                  if (broadcast)
                        _remote_net_broadcast->broadcast_transaction(tx);
                  return tx;
            }
            FC_CAPTURE_AND_RETHROW((name)(owner)(active)(registrar_account)(broadcast))
      }

      signed_transaction upgrade_account(string name, bool broadcast)
      {
            try
            {
                  FC_ASSERT(!self.is_locked());
                  account_object account_obj = get_account(name);
                  FC_ASSERT(!account_obj.is_lifetime_member());

                  signed_transaction tx;
                  account_upgrade_operation op;
                  op.account_to_upgrade = account_obj.get_id();
                  op.upgrade_to_lifetime_member = true;
                  tx.operations = {op};
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((name))
      }

      // This function generates derived keys starting with index 0 and keeps incrementing
      // the index until it finds a key that isn't registered in the block chain.  To be
      // safer, it continues checking for a few more keys to make sure there wasn't a short gap
      // caused by a failed registration or the like.
      int find_first_unused_derived_key_index(const fc::ecc::private_key &parent_key)
      {
            int first_unused_index = 0;
            int number_of_consecutive_unused_keys = 0;
            for (int key_index = 0;; ++key_index)
            {
                  fc::ecc::private_key derived_private_key = derive_private_key(key_to_wif(parent_key), key_index);
                  graphene::chain::public_key_type derived_public_key = derived_private_key.get_public_key();
                  if (_keys.find(derived_public_key) == _keys.end())
                  {
                        if (number_of_consecutive_unused_keys)
                        {
                              ++number_of_consecutive_unused_keys;
                              if (number_of_consecutive_unused_keys > 5)
                                    return first_unused_index;
                        }
                        else
                        {
                              first_unused_index = key_index;
                              number_of_consecutive_unused_keys = 1;
                        }
                  }
                  else
                  {
                        // key_index is used
                        first_unused_index = 0;
                        number_of_consecutive_unused_keys = 0;
                  }
            }
      }

      signed_transaction create_account_with_private_key(fc::ecc::private_key owner_privkey,
                                                         string account_name,
                                                         string registrar_account,
                                                         bool broadcast = false,
                                                         bool save_wallet = true)
      {
            try
            {
                  int active_key_index = find_first_unused_derived_key_index(owner_privkey);
                  fc::ecc::private_key active_privkey = derive_private_key(key_to_wif(owner_privkey), active_key_index);

                  int memo_key_index = find_first_unused_derived_key_index(active_privkey);
                  fc::ecc::private_key memo_privkey = derive_private_key(key_to_wif(active_privkey), memo_key_index);

                  graphene::chain::public_key_type owner_pubkey = owner_privkey.get_public_key();
                  graphene::chain::public_key_type active_pubkey = active_privkey.get_public_key();
                  graphene::chain::public_key_type memo_pubkey = memo_privkey.get_public_key();

                  account_create_operation account_create_op;

                  // TODO:  process when pay_from_account is ID

                  account_object registrar_account_object = get_account(registrar_account);

                  account_id_type registrar_account_id = registrar_account_object.id;

                  account_create_op.registrar = registrar_account_id;
                  account_create_op.name = account_name;
                  account_create_op.owner = authority(1, owner_pubkey, 1);
                  account_create_op.active = authority(1, active_pubkey, 1);
                  account_create_op.options.memo_key = memo_pubkey;

                  // current_fee_schedule()
                  // find_account(pay_from_account)

                  // account_create_op.fee = account_create_op.calculate_fee(db.current_fee_schedule());

                  signed_transaction tx;

                  tx.operations.push_back(account_create_op);

                  vector<public_key_type> paying_keys = registrar_account_object.active.get_keys();

                  auto dyn_props = get_dynamic_global_properties();
                  tx.set_reference_block(dyn_props.head_block_id);
                  tx.set_expiration(dyn_props.time + fc::seconds(30));
                  tx.validate();

                  for (public_key_type &key : paying_keys)
                  {
                        auto it = _keys.find(key);
                        if (it != _keys.end())
                        {
                              fc::optional<fc::ecc::private_key> privkey = wif_to_key(it->second);
                              FC_ASSERT(privkey.valid(), "Malformed private key in _keys");
                              tx.sign(*privkey, _chain_id);
                        }
                  }

                  // we do not insert owner_privkey here because
                  //   it is intended to only be used for key recovery
                  _wallet.pending_account_registrations[account_name].push_back(key_to_wif(active_privkey));
                  _wallet.pending_account_registrations[account_name].push_back(key_to_wif(memo_privkey));
                  if (save_wallet)
                        save_wallet_file();
                  if (broadcast)
                        _remote_net_broadcast->broadcast_transaction(tx);
                  return tx;
            }
            FC_CAPTURE_AND_RETHROW((account_name)(registrar_account)(broadcast))
      }

      signed_transaction create_account_with_brain_key(string brain_key,
                                                       string account_name,
                                                       string registrar_account,
                                                       bool broadcast = false,
                                                       bool save_wallet = true)
      {
            try
            {
                  FC_ASSERT(!self.is_locked());
                  string normalized_brain_key = normalize_brain_key(brain_key);
                  // TODO:  scan blockchain for accounts that exist with same brain key
                  fc::ecc::private_key owner_privkey = derive_private_key(normalized_brain_key, 0);
                  return create_account_with_private_key(owner_privkey, account_name, registrar_account, broadcast, save_wallet);
            }
            FC_CAPTURE_AND_RETHROW((account_name)(registrar_account))
      }

      signed_transaction create_asset(string issuer,
                                      string symbol,
                                      uint8_t precision,
                                      asset_options common,
                                      fc::optional<bitasset_options> bitasset_opts,
                                      bool broadcast = false)
      {
            try
            {
                  account_object issuer_account = get_account(issuer);
                  FC_ASSERT(!find_asset(symbol).valid(), "Asset with that symbol already exists!");

                  asset_create_operation create_op;
                  create_op.issuer = issuer_account.id;
                  create_op.symbol = symbol;
                  create_op.precision = precision;
                  create_op.common_options = common;
                  create_op.bitasset_opts = bitasset_opts;

                  signed_transaction tx;
                  tx.operations.push_back(create_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((issuer)(symbol)(precision)(common)(bitasset_opts)(broadcast))
      }

      signed_transaction update_asset(string symbol,
                                      fc::optional<string> new_issuer,
                                      asset_options new_options,
                                      bool broadcast /* = false */)
      {
            try
            {
                  fc::optional<asset_object> asset_to_update = find_asset(symbol);
                  if (!asset_to_update)
                        FC_THROW("No asset with that symbol exists!");
                  fc::optional<account_id_type> new_issuer_account_id;
                  if (new_issuer)
                  {
                        account_object new_issuer_account = get_account(*new_issuer);
                        new_issuer_account_id = new_issuer_account.id;
                  }

                  asset_update_operation update_op;
                  update_op.issuer = asset_to_update->issuer;
                  update_op.asset_to_update = asset_to_update->id;
                  update_op.new_issuer = new_issuer_account_id;
                  update_op.new_options = new_options;

                  signed_transaction tx;
                  tx.operations.push_back(update_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((symbol)(new_issuer)(new_options)(broadcast))
      }

      signed_transaction update_bitasset(string symbol,
                                         bitasset_options new_options,
                                         bool broadcast /* = false */)
      {
            try
            {
                  fc::optional<asset_object> asset_to_update = find_asset(symbol);
                  if (!asset_to_update)
                        FC_THROW("No asset with that symbol exists!");

                  asset_update_bitasset_operation update_op;
                  update_op.issuer = asset_to_update->issuer;
                  update_op.asset_to_update = asset_to_update->id;
                  update_op.new_options = new_options;

                  signed_transaction tx;
                  tx.operations.push_back(update_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((symbol)(new_options)(broadcast))
      }

      signed_transaction update_asset_feed_producers(string symbol,
                                                     flat_set<string> new_feed_producers,
                                                     bool broadcast /* = false */)
      {
            try
            {
                  fc::optional<asset_object> asset_to_update = find_asset(symbol);
                  if (!asset_to_update)
                        FC_THROW("No asset with that symbol exists!");

                  asset_update_feed_producers_operation update_op;
                  update_op.issuer = asset_to_update->issuer;
                  update_op.asset_to_update = asset_to_update->id;
                  update_op.new_feed_producers.reserve(new_feed_producers.size());
                  std::transform(new_feed_producers.begin(), new_feed_producers.end(),
                                 std::inserter(update_op.new_feed_producers, update_op.new_feed_producers.end()),
                                 [this](const std::string &account_name_or_id) { return get_account_id(account_name_or_id); });

                  signed_transaction tx;
                  tx.operations.push_back(update_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((symbol)(new_feed_producers)(broadcast))
      }

      signed_transaction publish_asset_feed(string publishing_account,
                                            string symbol,
                                            price_feed feed,
                                            bool broadcast /* = false */)
      {
            try
            {
                  fc::optional<asset_object> asset_to_update = find_asset(symbol);
                  if (!asset_to_update)
                        FC_THROW("No asset with that symbol exists!");

                  asset_publish_feed_operation publish_op;
                  publish_op.publisher = get_account_id(publishing_account);
                  publish_op.asset_id = asset_to_update->id;
                  publish_op.feed = feed;

                  signed_transaction tx;
                  tx.operations.push_back(publish_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((publishing_account)(symbol)(feed)(broadcast))
      }
      signed_transaction reserve_asset(string from,
                                       string amount,
                                       string symbol,
                                       bool broadcast /* = false */)
      {
            try
            {
                  account_object from_account = get_account(from);
                  fc::optional<asset_object> asset_to_reserve = find_asset(symbol);
                  if (!asset_to_reserve)
                        FC_THROW("No asset with that symbol exists!");

                  asset_reserve_operation reserve_op;
                  reserve_op.payer = from_account.id;
                  reserve_op.amount_to_reserve = asset_to_reserve->amount_from_string(amount);

                  signed_transaction tx;
                  tx.operations.push_back(reserve_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((from)(amount)(symbol)(broadcast))
      }

      signed_transaction global_settle_asset(string symbol,
                                             price settle_price,
                                             bool broadcast /* = false */)
      {
            try
            {
                  fc::optional<asset_object> asset_to_settle = find_asset(symbol);
                  if (!asset_to_settle)
                        FC_THROW("No asset with that symbol exists!");

                  asset_global_settle_operation settle_op;
                  settle_op.issuer = asset_to_settle->issuer;
                  settle_op.asset_to_settle = asset_to_settle->id;
                  settle_op.settle_price = settle_price;

                  signed_transaction tx;
                  tx.operations.push_back(settle_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((symbol)(settle_price)(broadcast))
      }

      signed_transaction settle_asset(string account_to_settle,
                                      string amount_to_settle,
                                      string symbol,
                                      bool broadcast /* = false */)
      {
            try
            {
                  fc::optional<asset_object> asset_to_settle = find_asset(symbol);
                  if (!asset_to_settle)
                        FC_THROW("No asset with that symbol exists!");

                  asset_settle_operation settle_op;
                  settle_op.account = get_account_id(account_to_settle);
                  settle_op.amount = asset_to_settle->amount_from_string(amount_to_settle);

                  signed_transaction tx;
                  tx.operations.push_back(settle_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((account_to_settle)(amount_to_settle)(symbol)(broadcast))
      }

      signed_transaction bid_collateral(string bidder_name,
                                        string debt_amount, string debt_symbol,
                                        string additional_collateral,
                                        bool broadcast)
      {
            try
            {
                  fc::optional<asset_object> debt_asset = find_asset(debt_symbol);
                  if (!debt_asset)
                        FC_THROW("No asset with that symbol exists!");
                  FC_ASSERT(debt_asset->bitasset_data_id.valid(), "debt_asset:${debt_asset},bitasset_data_id is null", ("debt_asset", *debt_asset));
                  const asset_object &collateral = get_asset(get_object(*debt_asset->bitasset_data_id).options.short_backing_asset);

                  bid_collateral_operation op;
                  op.bidder = get_account_id(bidder_name);
                  op.debt_covered = debt_asset->amount_from_string(debt_amount);
                  op.additional_collateral = collateral.amount_from_string(additional_collateral);

                  signed_transaction tx;
                  tx.operations.push_back(op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((bidder_name)(debt_amount)(debt_symbol)(additional_collateral)(broadcast))
      }
      /*
      signed_transaction whitelist_account(string authorizing_account,
                                           string account_to_list,
                                           account_whitelist_operation::account_listing new_listing_status,
                                           bool broadcast )
      {
            try
            {
                  account_whitelist_operation whitelist_op;
                  whitelist_op.authorizing_account = get_account_id(authorizing_account);
                  whitelist_op.account_to_list = get_account_id(account_to_list);
                  whitelist_op.new_listing = new_listing_status;

                  signed_transaction tx;
                  tx.operations.push_back(whitelist_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((authorizing_account)(account_to_list)(new_listing_status)(broadcast))
      }
*/
      signed_transaction create_committee_member(string owner_account, string url,
                                                 bool broadcast /* = false */)
      {
            try
            {

                  committee_member_create_operation committee_member_create_op;
                  committee_member_create_op.committee_member_account = get_account_id(owner_account);
                  committee_member_create_op.url = url;
                  if (_remote_db->get_committee_member_by_account(committee_member_create_op.committee_member_account))
                        FC_THROW("Account ${owner_account} is already a committee_member", ("owner_account", owner_account));

                  signed_transaction tx;
                  tx.operations.push_back(committee_member_create_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((owner_account)(broadcast))
      }

      signed_transaction update_committee_member(string committee_name, string url, bool work_status, bool broadcast /*= false*/)
      {
            committee_member_update_operation committee_member_update_op;
            committee_member_update_op.work_status = work_status;
            committee_member_update_op.new_url = url;
            committee_member_update_op.committee_member_account = get_account_id(committee_name);
            committee_member_update_op.committee_member = get_committee_member(committee_name).id;
            signed_transaction tx;
            tx.operations.push_back(committee_member_update_op);
            tx.validate();
            return sign_transaction(tx, broadcast);
      }
      witness_object get_witness(string owner_account)
      {
            try
            {
                  fc::optional<witness_id_type> witness_id = maybe_id<witness_id_type>(owner_account);
                  if (witness_id)
                  {
                        std::vector<witness_id_type> ids_to_get;
                        ids_to_get.push_back(*witness_id);
                        std::vector<fc::optional<witness_object>> witness_objects = _remote_db->get_witnesses(ids_to_get);
                        if (witness_objects.front())
                              return *witness_objects.front();
                        FC_THROW("No witness is registered for id ${id}", ("id", owner_account));
                  }
                  else
                  {
                        // then maybe it's the owner account
                        try
                        {
                              account_id_type owner_account_id = get_account_id(owner_account);
                              fc::optional<witness_object> witness = _remote_db->get_witness_by_account(owner_account_id);
                              if (witness)
                                    return *witness;
                              else
                                    FC_THROW("No witness is registered for account ${account}", ("account", owner_account));
                        }
                        catch (const fc::exception &)
                        {
                              FC_THROW("No account or witness named ${account}", ("account", owner_account));
                        }
                  }
            }
            FC_CAPTURE_AND_RETHROW((owner_account))
      }

      committee_member_object get_committee_member(string owner_account)
      {
            try
            {
                  fc::optional<committee_member_id_type> committee_member_id = maybe_id<committee_member_id_type>(owner_account);
                  if (committee_member_id)
                  {
                        std::vector<committee_member_id_type> ids_to_get;
                        ids_to_get.push_back(*committee_member_id);
                        std::vector<fc::optional<committee_member_object>> committee_member_objects = _remote_db->get_committee_members(ids_to_get);
                        if (committee_member_objects.front())
                              return *committee_member_objects.front();
                        FC_THROW("No committee_member is registered for id ${id}", ("id", owner_account));
                  }
                  else
                  {
                        // then maybe it's the owner account
                        try
                        {
                              account_id_type owner_account_id = get_account_id(owner_account);
                              fc::optional<committee_member_object> committee_member = _remote_db->get_committee_member_by_account(owner_account_id);
                              if (committee_member)
                                    return *committee_member;
                              else
                                    FC_THROW("No committee_member is registered for account ${account}", ("account", owner_account));
                        }
                        catch (const fc::exception &)
                        {
                              FC_THROW("No account or committee_member named ${account}", ("account", owner_account));
                        }
                  }
            }
            FC_CAPTURE_AND_RETHROW((owner_account))
      }

      signed_transaction create_witness(string owner_account,
                                        string url,
                                        bool broadcast /* = false */)
      {
            try
            {
                  account_object witness_account = get_account(owner_account);
                  fc::ecc::private_key active_private_key = get_private_key_for_account(witness_account);
                  int witness_key_index = find_first_unused_derived_key_index(active_private_key);
                  fc::ecc::private_key witness_private_key = derive_private_key(key_to_wif(active_private_key) + fc::sha224::hash(witness_account.name).str(), witness_key_index);
                  graphene::chain::public_key_type witness_public_key = witness_private_key.get_public_key();
                  wdump((witness_public_key)(witness_private_key));
                  witness_create_operation witness_create_op;
                  witness_create_op.witness_account = witness_account.id;
                  witness_create_op.block_signing_key = witness_public_key;
                  witness_create_op.url = url;

                  if (_remote_db->get_witness_by_account(witness_create_op.witness_account))
                        FC_THROW("Account ${owner_account} is already a witness", ("owner_account", owner_account));

                  signed_transaction tx;
                  tx.operations.push_back(witness_create_op);
                  tx.validate();
                  _keys.insert(make_pair(witness_public_key, key_to_wif(witness_private_key)));
                  _wallet.pending_witness_registrations[owner_account] = key_to_wif(witness_private_key);

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((owner_account)(broadcast))
      }

      signed_transaction update_witness(string witness_name,
                                        string url,
                                        string block_signing_key,
                                        bool work_status,
                                        bool broadcast /* = false */)
      {
            try
            {
                  witness_object witness = get_witness(witness_name);
                  account_object witness_account = get_account(witness.witness_account);
                  fc::ecc::private_key active_private_key = get_private_key_for_account(witness_account);

                  witness_update_operation witness_update_op;
                  witness_update_op.witness = witness.id;
                  witness_update_op.witness_account = witness_account.id;
                  witness_update_op.work_status = work_status;
                  if (url != "")
                        witness_update_op.new_url = url;
                  if (block_signing_key != "")
                        witness_update_op.new_signing_key = public_key_type(block_signing_key);

                  signed_transaction tx;
                  tx.operations.push_back(witness_update_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((witness_name)(url)(block_signing_key)(broadcast))
      }

      vector<vesting_balance_object_with_info> get_vesting_balances(string account_name)
      {
            try
            {
                  fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>(account_name);
                  std::vector<vesting_balance_object_with_info> result;
                  fc::time_point_sec now = _remote_db->get_dynamic_global_properties().time;

                  if (vbid)
                  {
                        result.emplace_back(get_object<vesting_balance_object>(*vbid), now);
                        return result;
                  }

                  // try casting to avoid a round-trip if we were given an account ID
                  fc::optional<account_id_type> acct_id = maybe_id<account_id_type>(account_name);
                  if (!acct_id)
                        acct_id = get_account(account_name).id;

                  vector<vesting_balance_object> vbos = _remote_db->get_vesting_balances(*acct_id);
                  if (vbos.size() == 0)
                        return result;

                  for (const vesting_balance_object &vbo : vbos)
                        result.emplace_back(vbo, now);

                  return result;
            }
            FC_CAPTURE_AND_RETHROW((account_name))
      }

      signed_transaction withdraw_vesting(
          string witness_name,
          string amount,
          string asset_symbol,
          bool broadcast = false)
      {
            try
            {
                  asset_object asset_obj = get_asset(asset_symbol);
                  fc::optional<vesting_balance_id_type> vbid = maybe_id<vesting_balance_id_type>(witness_name);
                  if (!vbid)
                  {
                        witness_object wit = get_witness(witness_name);
                        FC_ASSERT(wit.pay_vb);
                        vbid = wit.pay_vb;
                  }

                  vesting_balance_object vbo = get_object<vesting_balance_object>(*vbid);
                  vesting_balance_withdraw_operation vesting_balance_withdraw_op;

                  vesting_balance_withdraw_op.vesting_balance = *vbid;
                  vesting_balance_withdraw_op.owner = vbo.owner;
                  vesting_balance_withdraw_op.amount = asset_obj.amount_from_string(amount);

                  signed_transaction tx;
                  tx.operations.push_back(vesting_balance_withdraw_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((witness_name)(amount))
      }

      signed_transaction vote_for_committee_member(string voting_account,
                                                   string committee_member,
                                                   uint64_t approve,
                                                   bool broadcast /* = false */)
      {
            try
            {
                  account_object voting_account_object = (_remote_db->get_full_accounts(vector<string>{voting_account}, false)).begin()->second.account;
                  account_id_type committee_member_owner_account_id = get_account_id(committee_member);
                  fc::optional<committee_member_object> committee_member_obj = _remote_db->get_committee_member_by_account(committee_member_owner_account_id);
                  if (!committee_member_obj)
                        FC_THROW("Account ${committee_member} is not registered as a committee_member", ("committee_member", committee_member));

                  if (approve)
                  {
                        for (auto vote_id = voting_account_object.options.votes.begin(); vote_id != voting_account_object.options.votes.end();)
                        {
                              if (vote_id->type() == vote_id_type::vote_type::witness)
                              {
                                    auto &temp = *vote_id;
                                    voting_account_object.options.votes.erase(temp);
                              }
                              else
                                    vote_id++;
                        }
                        voting_account_object.options.votes.insert(committee_member_obj->vote_id);
                  }
                  else
                  {
                        voting_account_object.options.votes.clear();
                  }
                  account_update_operation account_update_op;
                  account_update_op.account = voting_account_object.id;
                  account_update_op.new_options = voting_account_object.options;
                  account_update_op.lock_with_vote = std::make_pair(vote_id_type::vote_type::committee, asset(approve));
                  signed_transaction tx;
                  tx.operations.push_back(account_update_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((voting_account)(committee_member)(approve)(broadcast))
      }

      signed_transaction vote_for_witness(string voting_account,
                                          string witness,
                                          int64_t approve,
                                          bool broadcast /* = false */)
      {
            try
            {
                  account_object voting_account_object = (_remote_db->get_full_accounts(vector<string>{voting_account}, false)).begin()->second.account;
                  ;
                  account_id_type witness_owner_account_id = get_account_id(witness);
                  fc::optional<witness_object> witness_obj = _remote_db->get_witness_by_account(witness_owner_account_id);
                  if (!witness_obj)
                        FC_THROW("Account ${witness} is not registered as a witness", ("witness", witness));

                  if (approve)
                  {
                        for (auto vote_id = voting_account_object.options.votes.begin(); vote_id != voting_account_object.options.votes.end();)
                        {
                              if (vote_id->type() == vote_id_type::vote_type::committee)
                              {
                                    auto &temp = *vote_id;
                                    voting_account_object.options.votes.erase(temp);
                              }
                              else
                                    vote_id++;
                        }
                        voting_account_object.options.votes.insert(witness_obj->vote_id);
                  }
                  else
                  {
                        voting_account_object.options.votes.clear();
                  }
                  account_update_operation account_update_op;
                  account_update_op.account = voting_account_object.id;
                  account_update_op.new_options = voting_account_object.options;
                  account_update_op.lock_with_vote = std::make_pair(vote_id_type::vote_type::witness, asset(approve));
                  signed_transaction tx;
                  tx.operations.push_back(account_update_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((voting_account)(witness)(approve)(broadcast))
      }

      signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false)
      {
            set<public_key_type> pks = _remote_db->get_potential_signatures(tx);
            flat_set<public_key_type> owned_keys;
            owned_keys.reserve(pks.size());
            std::copy_if(pks.begin(), pks.end(), std::inserter(owned_keys, owned_keys.end()),
                         [this](const public_key_type &pk) { return _keys.find(pk) != _keys.end(); });
            set<public_key_type> approving_key_set = _remote_db->get_required_signatures(tx, owned_keys);

            auto dyn_props = get_dynamic_global_properties();
            tx.set_reference_block(_remote_db->get_block_header(dyn_props.last_irreversible_block_num)->previous);

            // first, some bookkeeping, expire old items from _recently_generated_transactions
            // since transactions include the head block id, we just need the index for keeping transactions unique
            // when there are multiple transactions in the same block.  choose a time period that should be at
            // least one block long, even in the worst case.  2 minutes ought to be plenty.
            fc::time_point_sec oldest_transaction_ids_to_track(dyn_props.time - fc::minutes(2));
            auto oldest_transaction_record_iter = _recently_generated_transactions.get<timestamp_index>().lower_bound(oldest_transaction_ids_to_track);
            auto begin_iter = _recently_generated_transactions.get<timestamp_index>().begin();
            _recently_generated_transactions.get<timestamp_index>().erase(begin_iter, oldest_transaction_record_iter);

            uint32_t expiration_time_offset = 1200;
            for (;;)
            {
                  tx.set_expiration(dyn_props.time + fc::seconds(30 + expiration_time_offset));
                  tx.signatures.clear();

                  for (const public_key_type &key : approving_key_set)
                        tx.sign(get_private_key(key), _chain_id);
                  //////////////////////////nico add/////////////////////////////
                  for (const private_key_type key : extern_private_keys)
                  {
                        tx.sign(key, _chain_id);
                  }
                  extern_private_keys.clear();
                  ///////////////////////////end//////////////////////////////////////
                  graphene::chain::transaction_id_type this_transaction_id = tx.id();
                  auto iter = _recently_generated_transactions.find(this_transaction_id);
                  if (iter == _recently_generated_transactions.end())
                  {
                        // we haven't generated this transaction before, the usual case
                        recently_generated_transaction_record this_transaction_record;
                        this_transaction_record.generation_time = dyn_props.time;
                        this_transaction_record.transaction_id = this_transaction_id;
                        _recently_generated_transactions.insert(this_transaction_record);
                        break;
                  }

                  // else we've generated a dupe, increment expiration time and re-sign it
                  ++expiration_time_offset;
            }

            if (broadcast)
            {
                  try
                  {
                        _remote_net_broadcast->broadcast_transaction(tx);
                  }
                  catch (const fc::exception &e)
                  {
                        elog("Caught exception while broadcasting tx ${id}:  ${e}", ("id", tx.id().str())("e", e.to_detail_string()));
                        throw;
                  }
            }

            return tx;
      }

      memo_data sign_memo(string from, string to, string memo)
      {
            FC_ASSERT(!self.is_locked());

            memo_data md = memo_data();

            // get account memo key, if that fails, try a pubkey
            try
            {
                  account_object from_account = get_account(from);
                  md.from = from_account.options.memo_key;
            }
            catch (const fc::exception &e)
            {
                  md.from = self.get_public_key(from);
            }
            // same as above, for destination key
            try
            {
                  account_object to_account = get_account(to);
                  md.to = to_account.options.memo_key;
            }
            catch (const fc::exception &e)
            {
                  md.to = self.get_public_key(to);
            }

            md.set_message(get_private_key(md.from), md.to, memo);
            return md;
      }

      string read_memo(const memo_data &md)
      {
            FC_ASSERT(!is_locked());
            std::string clear_text;

            const memo_data *memo = &md;

            try
            {
                  FC_ASSERT(_keys.count(memo->to) || _keys.count(memo->from), "Memo is encrypted to a key ${to} or ${from} not in this wallet.", ("to", memo->to)("from", memo->from));
                  if (_keys.count(memo->to))
                  {
                        auto my_key = wif_to_key(_keys.at(memo->to));
                        FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
                        clear_text = memo->get_message(*my_key, memo->from);
                  }
                  else
                  {
                        auto my_key = wif_to_key(_keys.at(memo->from));
                        FC_ASSERT(my_key, "Unable to recover private key to decrypt memo. Wallet may be corrupted.");
                        clear_text = memo->get_message(*my_key, memo->to);
                  }
            }
            catch (const fc::exception &e)
            {
                  elog("Error when decrypting memo: ${e}", ("e", e.to_detail_string()));
            }

            return clear_text;
      }

      signed_transaction sell_asset(string seller_account,
                                    string amount_to_sell,
                                    string symbol_to_sell,
                                    string min_to_receive,
                                    string symbol_to_receive,
                                    uint32_t timeout_sec = 0,
                                    bool fill_or_kill = false,
                                    bool broadcast = false)
      {
            account_object seller = get_account(seller_account);

            limit_order_create_operation op;
            op.seller = seller.id;
            op.amount_to_sell = get_asset(symbol_to_sell).amount_from_string(amount_to_sell);
            op.min_to_receive = get_asset(symbol_to_receive).amount_from_string(min_to_receive);
            if (timeout_sec)
                  op.expiration = fc::time_point::now() + fc::seconds(timeout_sec);
            op.fill_or_kill = fill_or_kill;

            signed_transaction tx;
            tx.operations.push_back(op);
            tx.validate();

            return sign_transaction(tx, broadcast);
      }

      signed_transaction borrow_asset(string seller_name, string amount_to_borrow, string asset_symbol,
                                      string amount_of_collateral, bool broadcast = false)
      {
            account_object seller = get_account(seller_name);
            asset_object mia = get_asset(asset_symbol);
            FC_ASSERT(mia.is_market_issued());
            asset_object collateral = get_asset(get_object(*mia.bitasset_data_id).options.short_backing_asset);

            call_order_update_operation op;
            op.funding_account = seller.id;
            op.delta_debt = mia.amount_from_string(amount_to_borrow);
            op.delta_collateral = collateral.amount_from_string(amount_of_collateral);

            signed_transaction trx;
            trx.operations = {op};
            trx.validate();
            idump((broadcast));

            return sign_transaction(trx, broadcast);
      }

      signed_transaction cancel_order(object_id_type order_id, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  FC_ASSERT(order_id.space() == protocol_ids, "Invalid order ID ${id}", ("id", order_id));
                  signed_transaction trx;

                  limit_order_cancel_operation op;
                  op.fee_paying_account = get_object<limit_order_object>(order_id).seller;
                  op.order = order_id;
                  trx.operations = {op};

                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((order_id))
      }

      signed_transaction transfer(string from, string to, string amount,
                                  string asset_symbol, pair<string, bool> memo, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!self.is_locked());
                  fc::optional<asset_object> asset_obj = get_asset(asset_symbol);
                  FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", asset_symbol));

                  account_object from_account = get_account(from);
                  account_object to_account = get_account(to);
                  account_id_type from_id = from_account.id;
                  account_id_type to_id = get_account_id(to);

                  transfer_operation xfer_op;

                  xfer_op.from = from_id;
                  xfer_op.to = to_id;
                  xfer_op.amount = asset_obj->amount_from_string(amount);

                  if (memo.first.size())
                  {
                        if (memo.second)
                        {
                              auto temp_memo = memo_data();
                              temp_memo.from = from_account.options.memo_key;
                              temp_memo.to = to_account.options.memo_key;
                              temp_memo.set_message(get_private_key(from_account.options.memo_key),
                                                    to_account.options.memo_key, memo.first);
                              xfer_op.memo = temp_memo;
                        }
                        else
                              xfer_op.memo = memo.first;
                  }

                  signed_transaction tx;
                  tx.operations.push_back(xfer_op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((from)(to)(amount)(asset_symbol)(memo)(broadcast))
      }

      /************************************************nico add**********************************************************************************/
      signed_transaction update_collateral_for_gas(account_id_type mortgager, account_id_type beneficiary, share_type collateral, bool broadcast = false)
      {

            try
            {
                  FC_ASSERT(!self.is_locked());
                  FC_ASSERT(collateral >= 0);
                  update_collateral_for_gas_operation op;
                  op.mortgager = mortgager;
                  op.beneficiary = beneficiary;
                  op.collateral = collateral;
                  signed_transaction tx;
                  tx.operations.push_back(op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((mortgager)(beneficiary)(collateral)(broadcast))
      }

      signed_transaction create_contract(string owner, string name, public_key_type contract_authority, string data, bool broadcast = false) // wallet 合约 API
      {
            try
            {
                  FC_ASSERT(!self.is_locked());

                  account_object owner_account = get_account(owner);
                  account_id_type owner_id = owner_account.id;

                  contract_create_operation op;

                  op.name = name;
                  op.owner = owner_id;
                  op.data = data;
                  op.contract_authority = contract_authority;

                  signed_transaction tx;
                  tx.operations.push_back(op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((owner)(name)(data)(broadcast))
      }

      signed_transaction revise_contract(string reviser, string contract_id_or_name, string data, bool broadcast = false) // wallet 合约 API
      {
            try
            {
                  FC_ASSERT(!self.is_locked());

                  account_object owner_account = get_account(reviser);
                  fc::optional<contract_object> contract = _remote_db->get_contract(contract_id_or_name);
                  account_id_type reviser_id = owner_account.id;

                  revise_contract_operation op;

                  op.reviser = reviser_id;
                  op.contract_id = contract->id;
                  op.data = data;

                  signed_transaction tx;
                  tx.operations.push_back(op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((reviser)(contract_id_or_name)(data)(broadcast))
      }

      signed_transaction call_contract_function(string account_id_or_name, string contract_id_or_name, string function_name,
                                                vector<lua_types> value_list, bool broadcast = false) // wallet 合约 API
      {
            try
            {
                  FC_ASSERT(!self.is_locked());

                  account_object caller = get_account(account_id_or_name);
                  fc::optional<contract_object> contract_itr = _remote_db->get_contract(contract_id_or_name);
                  contract_object contract = *contract_itr;
                  call_contract_function_operation op;

                  op.caller = caller.id;
                  op.contract_id = contract.id;
                  op.function_name = function_name;
                  op.value_list = value_list;

                  signed_transaction tx;
                  tx.operations.push_back(op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((account_id_or_name)(contract_id_or_name)(function_name)(value_list)(broadcast))
      }

      signed_transaction adjustment_temporary_authorization(string account_id_or_name, string describe, fc::time_point_sec expiration_time,
                                                            flat_map<public_key_type, weight_type> temporary_active, bool broadcast = false) //nico add :: wallet 合约 API
      {
            try
            {
                  FC_ASSERT(!self.is_locked());

                  account_object caller = get_account(account_id_or_name);

                  temporary_authority_change_operation op;

                  op.owner = caller.id;
                  op.describe = describe;
                  op.temporary_active = temporary_active;
                  op.expiration_time = expiration_time;

                  signed_transaction tx;
                  tx.operations.push_back(op);
                  tx.validate();

                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((account_id_or_name)(describe)(temporary_active)(broadcast))
      }

      signed_transaction register_nh_asset_creator(const string &fee_paying_account, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  register_nh_asset_creator_operation regi_op;
                  regi_op.fee_paying_account = get_account(fee_paying_account).id;

                  signed_transaction tx;
                  tx.operations.push_back(regi_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((fee_paying_account))
      }

      signed_transaction create_world_view(const string &fee_paying_account, const string &world_view, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  create_world_view_operation cr_op;
                  cr_op.fee_paying_account = get_account(fee_paying_account).id;
                  cr_op.world_view = world_view;

                  signed_transaction tx;
                  tx.operations.push_back(cr_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((fee_paying_account)(world_view))
      }

      signed_transaction propose_relate_world_view(const string &proposing_account, fc::time_point_sec expiration_time,
                                                   const string &world_view_owner, const string &world_view, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  fc::optional<world_view_object> ver_obj = _remote_db->lookup_world_view({world_view})[0];
                  FC_ASSERT(ver_obj, "the nh asset is not exist");

                  relate_world_view_operation relate_op;
                  relate_op.related_account = get_account(proposing_account).id;
                  relate_op.view_owner = get_account(world_view_owner).id;
                  relate_op.world_view = ver_obj->world_view;

                  proposal_create_operation prop_op;

                  prop_op.expiration_time = expiration_time;
                  //prop_op.review_period_seconds = 0;
                  prop_op.fee_paying_account = get_account(proposing_account).id;

                  prop_op.proposed_ops.emplace_back(relate_op);

                  signed_transaction tx;
                  tx.operations.push_back(prop_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((proposing_account)(expiration_time)(world_view_owner)(world_view))
      }

      signed_transaction create_nh_asset(const string &creator, const string &owner, const string &asset_id, const string &world_view,
                                         const string &base_describe, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());

                  fc::optional<asset_object> asset_obj = get_asset(asset_id);
                  FC_ASSERT(asset_obj, "Could not find asset matching ${asset}", ("asset", asset_id));

                  fc::optional<world_view_object> ver_obj = _remote_db->lookup_world_view({world_view})[0];
                  FC_ASSERT(ver_obj, "Could not find nh asset matching ${world_view}", ("world_view", world_view));

                  const chain_parameters &current_params = get_global_properties().parameters;

                  create_nh_asset_operation create_op;
                  create_op.fee_paying_account = get_account(creator).id;
                  create_op.owner = get_account(owner).id;
                  create_op.asset_id = asset_obj->symbol;
                  create_op.world_view = ver_obj->world_view;
                  create_op.base_describe = base_describe;

                  signed_transaction tx;
                  tx.operations.push_back(create_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((creator)(owner)(asset_id)(world_view)(base_describe))
      }

      signed_transaction transfer_nh_asset(const string &from, const string &to, const string &nh_asset, bool broadcast)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  auto nh_asset_obj = _remote_db->lookup_nh_asset({nh_asset}).front();
                  FC_ASSERT(nh_asset_obj, "Could not find nh asset matching ${nh_asset}", ("nh_asset", nh_asset));

                  transfer_nh_asset_operation tr_op;
                  tr_op.from = get_account(from).id;
                  tr_op.to = get_account(to).id;
                  tr_op.nh_asset = nh_asset_obj->id;

                  signed_transaction tx;
                  tx.operations.push_back(tr_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((from)(to)(nh_asset))
      }
      /*
      signed_transaction relate_nh_asset(const string &nh_asset_creator, const string &parent, const string &child,
                                         const string &contract_id_or_name, bool relate, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  auto parent_nh_asset_obj = _remote_db->lookup_nh_asset({parent}).front();
                  FC_ASSERT(parent_nh_asset_obj, "Could not find nh asset matching ${nh_asset}", ("nh_asset", parent));

                  auto child_nh_asset_obj = _remote_db->lookup_nh_asset({child}).front();
                  FC_ASSERT(child_nh_asset_obj, "Could not find nh asset  matching ${nh_asset}", ("nh_asset", child));
                  auto contract = *_remote_db->get_contract(contract_id_or_name);
                  signed_transaction trx;
                  relate_nh_asset_operation op;
                  op.nh_asset_creator = get_account(nh_asset_creator).id;
                  op.parent = parent_nh_asset_obj->id;
                  op.child = child_nh_asset_obj->id;
                  op.contract = contract.id;
                  op.relate = relate;
                  trx.operations = {op};
                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((nh_asset_creator)(parent)(child)(contract_id_or_name)(relate))
      }
*/
      signed_transaction delete_nh_asset(
          const string &fee_paying_account,
          const string &nh_asset,
          bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  auto nh_asset_obj = _remote_db->lookup_nh_asset({nh_asset}).front();
                  FC_ASSERT(nh_asset_obj, "Could not find nh asset matching ${nh_asset}", ("nh_asset", nh_asset));

                  delete_nh_asset_operation del_op;
                  del_op.fee_paying_account = get_account(fee_paying_account).id;
                  del_op.nh_asset = nh_asset_obj->id;

                  signed_transaction tx;
                  tx.operations.push_back(del_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((fee_paying_account)(nh_asset))
      }

      signed_transaction create_nh_asset_order(const string &seller, const string &otcaccount, const string &pending_fee_amount, const string &pending_fee_symbol,
                                               const string &nh_asset, const string &price_amount, const string &price_symbol, const string &memo, fc::time_point_sec expiration_time,
                                               bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());

                  const chain_parameters &current_params = get_global_properties().parameters;

                  fc::optional<asset_object> pending_fee_obj = get_asset(pending_fee_symbol);
                  FC_ASSERT(pending_fee_obj, "Could not find asset matching ${asset}", ("asset", pending_fee_symbol));

                  fc::optional<asset_object> price_obj = get_asset(price_symbol);
                  FC_ASSERT(price_obj, "Could not find asset matching ${asset}", ("asset", price_symbol));

                  auto nh_asset_obj = _remote_db->lookup_nh_asset({nh_asset}).front();
                  FC_ASSERT(nh_asset_obj, "Could not find nh_asset matching ${nh_asset}", ("nh_asset", nh_asset));

                  create_nh_asset_order_operation cr_op;
                  cr_op.seller = get_account(seller).id;
                  cr_op.otcaccount = get_account(otcaccount).id;
                  cr_op.pending_orders_fee = pending_fee_obj->amount_from_string(pending_fee_amount);
                  cr_op.nh_asset = nh_asset_obj->id;
                  cr_op.price = price_obj->amount_from_string(price_amount);
                  cr_op.memo = memo;
                  cr_op.expiration = expiration_time;

                  signed_transaction tx;
                  tx.operations.push_back(cr_op);
                  tx.validate();
                  return sign_transaction(tx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((seller)(otcaccount)(pending_fee_amount)(pending_fee_symbol)(nh_asset)(price_amount)(price_symbol)(memo)(expiration_time))
      }

      signed_transaction cancel_nh_asset_order(object_id_type order_id, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  FC_ASSERT(order_id.space() == nh_asset_protocol_ids && order_id.type() == nh_asset_order_object_type, "Invalid order ID ${id}", ("id", order_id));
                  signed_transaction trx;

                  cancel_nh_asset_order_operation op;
                  op.fee_paying_account = get_object<nh_asset_order_object>(order_id).seller;
                  op.order = order_id;
                  trx.operations = {op};

                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((order_id))
      }

      signed_transaction fill_nh_asset_order(const string &fee_paying_account, nh_asset_order_id_type order_id, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());

                  const nh_asset_order_object &order_obj = get_object(order_id);
                  signed_transaction trx;
                  fill_nh_asset_order_operation op;
                  op.fee_paying_account = get_account(fee_paying_account).id;
                  op.order = order_id;
                  op.seller = order_obj.seller;
                  op.nh_asset = order_obj.nh_asset_id;
                  op.price_amount = get_asset(order_obj.price.asset_id).amount_to_string(order_obj.price);
                  op.price_asset_id = order_obj.price.asset_id;
                  op.price_asset_symbol = get_asset(order_obj.price.asset_id).symbol;
                  trx.operations = {op};

                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((order_id))
      }

      /*********************** file **************************************/
      signed_transaction create_file(const string &file_creator, const string &file_name, const string &file_content, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  signed_transaction trx;
                  create_file_operation op;
                  op.file_owner = get_account(file_creator).id;
                  op.file_name = file_name;
                  op.file_content = file_content;

                  trx.operations = {op};

                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((file_creator)(file_name)(file_content))
      }

      signed_transaction add_file_relate_account(const string &file_creator, const string &file_name_or_id, const vector<string> &related_account, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());

                  fc::optional<file_object> file_obj = _remote_db->lookup_file(file_name_or_id);
                  FC_ASSERT(file_obj, "the file is not exist");

                  signed_transaction trx;
                  add_file_relate_account_operation op;
                  op.file_owner = get_account(file_creator).id;
                  op.file_id = file_obj->id;
                  std::transform(related_account.begin(), related_account.end(), std::inserter(op.related_account, op.related_account.end()),
                                 [&](const string &account) { return get_account(account).id; });

                  trx.operations = {op};
                  trx.validate();

                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((file_creator)(file_name_or_id)(related_account))
      }

      signed_transaction file_signature(const string &signature_account, const string &file_name_or_id, const string &signature, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());

                  fc::optional<file_object> file_obj = _remote_db->lookup_file(file_name_or_id);
                  FC_ASSERT(file_obj, "the file is not exist");

                  signed_transaction trx;
                  file_signature_operation op;
                  op.signature_account = get_account(signature_account).id;
                  op.file_id = file_obj->id;
                  op.signature = signature;

                  trx.operations = {op};
                  trx.validate();

                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((signature_account)(file_name_or_id)(signature))
      }

      signed_transaction propose_relate_parent_file(const string &file_creator, const string &parent_file_name_or_id,
                                                    const string &sub_file_name_or_id, fc::time_point_sec expiration_time, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  const chain_parameters &current_params = get_global_properties().parameters;

                  fc::optional<file_object> parent_file_obj = _remote_db->lookup_file(parent_file_name_or_id);
                  FC_ASSERT(parent_file_obj, "the parent file is not exist");

                  fc::optional<file_object> sub_file_obj = _remote_db->lookup_file(sub_file_name_or_id);
                  FC_ASSERT(sub_file_obj, "the sub file is not exist");
                  signed_transaction trx;
                  relate_parent_file_operation op;
                  op.sub_file_owner = get_account(file_creator).id;
                  op.parent_file = parent_file_obj->id;
                  op.parent_file_owner = parent_file_obj->file_owner;
                  op.sub_file = sub_file_obj->id;

                  proposal_create_operation prop_op;
                  prop_op.expiration_time = expiration_time;
                  //prop_op.review_period_seconds = 0;
                  prop_op.fee_paying_account = get_account(file_creator).id;

                  prop_op.proposed_ops.emplace_back(op);
                  trx.operations = {prop_op};

                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((file_creator)(parent_file_name_or_id)(sub_file_name_or_id)(expiration_time))
      }

      /***********************file end**************************************/
      /*********************** crontab start *******************************/
      signed_transaction create_crontab(const string &task_creator, fc::time_point_sec start_time, uint16_t execute_interval,
                                        uint64_t target_execute_times, const vector<operation> &crontab_ops, bool broadcast = false)
      {
            try
            {
                  FC_ASSERT(!is_locked());

                  const chain_parameters &current_params = get_global_properties().parameters;
                  signed_transaction trx;
                  crontab_create_operation op;
                  op.crontab_creator = get_account(task_creator).id;
                  op.start_time = start_time;
                  op.execute_interval = execute_interval;
                  op.scheduled_execute_times = target_execute_times;
                  for (auto iter = crontab_ops.begin(); iter != crontab_ops.end(); iter++)
                  {
                        op.crontab_ops.emplace_back(*iter);
                  }
                  trx.operations = {op};
                  trx.validate();

                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((task_creator)(start_time)(execute_interval)(target_execute_times)(crontab_ops))
      }

      signed_transaction crontab_builder_transaction(transaction_handle_type handle, string account_name_or_id, fc::time_point_sec start_time,
                                                     uint16_t execute_interval, uint64_t target_execute_times, bool broadcast = true)
      {
            FC_ASSERT(_builder_transactions.count(handle));
            crontab_create_operation op;
            op.crontab_creator = get_account(account_name_or_id).get_id();
            op.start_time = start_time;
            op.execute_interval = execute_interval;
            op.scheduled_execute_times = target_execute_times;
            signed_transaction trx = _builder_transactions[handle];
            std::transform(trx.operations.begin(), trx.operations.end(), std::back_inserter(op.crontab_ops),
                           [](const operation &op) -> op_wrapper { return op; });
            trx.operations = {op};
            return trx = sign_transaction(trx, broadcast);
      }

      signed_transaction cancel_crontab(const string &task_creator, object_id_type task_id, bool broadcast)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  FC_ASSERT(task_id.space() == protocol_ids && task_id.type() == crontab_object_type, "Invalid crontab ID ${id}", ("id", task_id));

                  const chain_parameters &current_params = get_global_properties().parameters;
                  signed_transaction trx;
                  crontab_cancel_operation op;
                  op.fee_paying_account = get_account(task_creator).id;
                  op.task = task_id;

                  trx.operations = {op};
                  trx.validate();

                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((task_creator)(task_id))
      }

      signed_transaction recover_crontab(const string &crontab_creator, object_id_type crontab_id, fc::time_point_sec restart_time, bool broadcast)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  FC_ASSERT(crontab_id.space() == protocol_ids && crontab_id.type() == crontab_object_type, "Invalid crontab ID ${id}", ("id", crontab_id));

                  const chain_parameters &current_params = get_global_properties().parameters;
                  signed_transaction trx;
                  crontab_recover_operation op;
                  op.crontab_owner = get_account(crontab_creator).id;
                  op.crontab = crontab_id;
                  op.restart_time = restart_time;

                  trx.operations = {op};
                  trx.validate();

                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((crontab_creator)(crontab_id)(restart_time))
      }

      signed_transaction asset_update_restricted_list(const string &asset_issuer, string target_asset, restricted_enum restricted_type, vector<object_id_type> restricted_list, bool isadd, bool broadcast)
      {
            try
            {
                  FC_ASSERT(!is_locked());
                  signed_transaction trx;
                  asset_update_restricted_operation op;
                  op.target_asset = get_asset(target_asset).id;
                  op.payer = get_account(asset_issuer).id;
                  op.restricted_type = restricted_type;
                  op.restricted_list = restricted_list;
                  op.isadd = isadd;
                  trx.operations = {op};
                  trx.validate();
                  return sign_transaction(trx, broadcast);
            }
            FC_CAPTURE_AND_RETHROW((asset_issuer)(target_asset)(restricted_type)(restricted_list)(isadd))
      }
      /***************************************************nico end*******************************************************************************/

      signed_transaction issue_asset(string to_account, string amount, string symbol, pair<string, bool> memo, bool broadcast = false)
      {
            auto asset_obj = get_asset(symbol);

            account_object to = get_account(to_account);
            account_object issuer = get_account(asset_obj.issuer);

            asset_issue_operation issue_op;
            issue_op.issuer = asset_obj.issuer;
            issue_op.asset_to_issue = asset_obj.amount_from_string(amount);
            issue_op.issue_to_account = to.id;

            if (memo.first.size())
            {
                  if (memo.second)
                  {
                        auto temp_memo = memo_data();
                        temp_memo.from = issuer.options.memo_key;
                        temp_memo.to = to.options.memo_key;
                        temp_memo.set_message(get_private_key(issuer.options.memo_key),
                                              to.options.memo_key, memo.first);
                        issue_op.memo = temp_memo;
                  }
                  else
                        issue_op.memo = memo.first;
            }

            signed_transaction tx;
            tx.operations.push_back(issue_op);
            tx.validate();

            return sign_transaction(tx, broadcast);
      }

      std::map<string, std::function<string(fc::variant, const fc::variants &)>> get_result_formatters() const
      {
            std::map<string, std::function<string(fc::variant, const fc::variants &)>> m;
            m["help"] = [](variant result, const fc::variants &a) {
                  return result.get_string();
            };

            m["gethelp"] = [](variant result, const fc::variants &a) {
                  return result.get_string();
            };

            m["get_account_history"] = [this](variant result, const fc::variants &a) {
                  auto r = result.as<vector<operation_detail>>();
                  std::stringstream ss;

                  for (operation_detail &d : r)
                  {
                        operation_history_object &i = d.op;
                        auto b = _remote_db->get_block_header(i.block_num);
                        FC_ASSERT(b);
                        ss << b->timestamp.to_iso_string() << " ";
                        i.op.visit(operation_printer(ss, *this, i.result));
                        ss << " \n";
                  }

                  return ss.str();
            };
            m["get_relative_account_history"] = [this](variant result, const fc::variants &a) {
                  auto r = result.as<vector<operation_detail>>();
                  std::stringstream ss;

                  for (operation_detail &d : r)
                  {
                        operation_history_object &i = d.op;
                        auto b = _remote_db->get_block_header(i.block_num);
                        FC_ASSERT(b);
                        ss << b->timestamp.to_iso_string() << " ";
                        i.op.visit(operation_printer(ss, *this, i.result));
                        ss << " \n";
                  }

                  return ss.str();
            };

            m["list_account_balances"] = [this](variant result, const fc::variants &a) {
                  auto r = result.as<vector<asset>>();
                  vector<asset_object> asset_recs;
                  std::transform(r.begin(), r.end(), std::back_inserter(asset_recs), [this](const asset &a) {
                        return get_asset(a.asset_id);
                  });

                  std::stringstream ss;
                  for (unsigned i = 0; i < asset_recs.size(); ++i)
                        ss << asset_recs[i].amount_to_pretty_string(r[i]) << "\n";

                  return ss.str();
            };
            m["get_order_book"] = [this](variant result, const fc::variants &a) {
                  auto orders = result.as<order_book>();
                  auto bids = orders.bids;
                  auto asks = orders.asks;
                  std::stringstream ss;
                  std::stringstream sum_stream;
                  sum_stream << "Sum(" << orders.base << ')';
                  double bid_sum = 0;
                  double ask_sum = 0;
                  const int spacing = 20;

                  auto prettify_num = [&](double n) {
                        //ss << n;
                        if (abs(round(n) - n) < 0.00000000001)
                        {
                              //ss << setiosflags( !ios::fixed ) << (int) n;     // doesn't compile on Linux with gcc
                              ss << (int)n;
                        }
                        else if (n - floor(n) < 0.000001)
                        {
                              ss << setiosflags(ios::fixed) << setprecision(10) << n;
                        }
                        else
                        {
                              ss << setiosflags(ios::fixed) << setprecision(6) << n;
                        }
                  };

                  ss << setprecision(8) << setiosflags(ios::fixed) << setiosflags(ios::left);

                  ss << ' ' << setw((spacing * 4) + 6) << "BUY ORDERS"
                     << "SELL ORDERS\n"
                     << ' ' << setw(spacing + 1) << "Price" << setw(spacing) << orders.quote << ' ' << setw(spacing)
                     << orders.base << ' ' << setw(spacing) << sum_stream.str()
                     << "   " << setw(spacing + 1) << "Price" << setw(spacing) << orders.quote << ' ' << setw(spacing)
                     << orders.base << ' ' << setw(spacing) << sum_stream.str()
                     << "\n====================================================================================="
                     << "|=====================================================================================\n";

                  for (unsigned int i = 0; i < bids.size() || i < asks.size(); i++)
                  {
                        if (i < bids.size())
                        {
                              bid_sum += bids[i].base;
                              ss << ' ' << setw(spacing);
                              prettify_num(bids[i].price);
                              ss << ' ' << setw(spacing);
                              prettify_num(bids[i].quote);
                              ss << ' ' << setw(spacing);
                              prettify_num(bids[i].base);
                              ss << ' ' << setw(spacing);
                              prettify_num(bid_sum);
                              ss << ' ';
                        }
                        else
                        {
                              ss << setw((spacing * 4) + 5) << ' ';
                        }

                        ss << '|';

                        if (i < asks.size())
                        {
                              ask_sum += asks[i].base;
                              ss << ' ' << setw(spacing);
                              prettify_num(asks[i].price);
                              ss << ' ' << setw(spacing);
                              prettify_num(asks[i].quote);
                              ss << ' ' << setw(spacing);
                              prettify_num(asks[i].base);
                              ss << ' ' << setw(spacing);
                              prettify_num(ask_sum);
                        }

                        ss << '\n';
                  }

                  ss << endl
                     << "Buy Total:  " << bid_sum << ' ' << orders.base << endl
                     << "Sell Total: " << ask_sum << ' ' << orders.base << endl;

                  return ss.str();
            };

            return m;
      }

      signed_transaction propose_parameter_change(const string &proposing_account, fc::time_point_sec expiration_time,
                                                  const variant_object &changed_values, fc::optional<proposal_id_type> proposal_base, bool broadcast = false)
      {
            FC_ASSERT(!changed_values.contains("current_fees"));
            chain_parameters current_params;
            bool on_proposal = false;
            if (proposal_base.valid())
            {
                  const proposal_object proposal_base_ob = get_object(*proposal_base);
                  for (auto &temp_op : proposal_base_ob.proposed_transaction.operations)
                  {
                        if (temp_op.which() == operation::tag<committee_member_update_global_parameters_operation>::value)
                        {
                              current_params = temp_op.get<committee_member_update_global_parameters_operation>().new_parameters;
                              on_proposal = true;
                              break;
                        }
                  }
            }
            if (!on_proposal)
                  current_params = get_global_properties().parameters;
            chain_parameters new_params = current_params;
            fc::reflector<chain_parameters>::visit(
                fc::from_variant_visitor<chain_parameters>(changed_values, new_params));

            committee_member_update_global_parameters_operation update_op;
            update_op.new_parameters = new_params;

            proposal_create_operation prop_op;

            prop_op.expiration_time = expiration_time;
            prop_op.review_period_seconds = current_params.committee_proposal_review_period;
            prop_op.fee_paying_account = get_account(proposing_account).id;

            prop_op.proposed_ops.emplace_back(update_op);

            signed_transaction tx;
            tx.operations.push_back(prop_op);
            tx.validate();

            return sign_transaction(tx, broadcast);
      }

      signed_transaction propose_fee_change(const string &proposing_account, fc::time_point_sec expiration_time,
                                            const variant_object &changed_fees, fc::optional<proposal_id_type> proposal_base, bool broadcast = false)
      {

            chain_parameters current_params;
            bool on_proposal = false;
            if (proposal_base.valid())
            {
                  const proposal_object proposal_base_ob = get_object(*proposal_base);
                  for (auto &temp_op : proposal_base_ob.proposed_transaction.operations)
                  {
                        if (temp_op.which() == operation::tag<committee_member_update_global_parameters_operation>::value)
                        {
                              current_params = temp_op.get<committee_member_update_global_parameters_operation>().new_parameters;
                              on_proposal = true;
                              break;
                        }
                  }
            }
            if (!on_proposal)
                  current_params = get_global_properties().parameters;
            const fee_schedule_type &current_fees = *(current_params.current_fees);

            flat_map<int, fee_parameters> fee_map;
            fee_map.reserve(current_fees.parameters.size());
            for (const fee_parameters &op_fee : current_fees.parameters)
                  fee_map[op_fee.which()] = op_fee;
            uint32_t scale = current_fees.scale;
            int64_t maximun_handling_fee = current_fees.maximun_handling_fee;

            for (const auto &item : changed_fees)
            {
                  const string &key = item.key();
                  if (key == "scale")
                  {
                        int64_t _scale = item.value().as_int64();
                        FC_ASSERT(_scale >= 0);
                        FC_ASSERT(_scale <= std::numeric_limits<uint32_t>::max());
                        scale = uint32_t(_scale);
                        continue;
                  }
                  if (key == "maximun_handling_fee")
                  {
                        int64_t _maximun_handling_fee = item.value().as_int64();
                        FC_ASSERT(_maximun_handling_fee >= 0);
                        maximun_handling_fee = _maximun_handling_fee;
                        continue;
                  }
                  // is key a number?
                  auto is_numeric = [&]() -> bool {
                        size_t n = key.size();
                        for (size_t i = 0; i < n; i++)
                        {
                              if (!isdigit(key[i]))
                                    return false;
                        }
                        return true;
                  };

                  int which;
                  if (is_numeric())
                        which = std::stoi(key);
                  else
                  {
                        const auto &n2w = _operation_which_map.name_to_which;
                        auto it = n2w.find(key);
                        FC_ASSERT(it != n2w.end(), "unknown operation");
                        which = it->second;
                  }

                  fee_parameters fp = from_which_variant<fee_parameters>(which, item.value());
                  fee_map[which] = fp;
            }

            fee_schedule_type new_fees;

            for (const std::pair<int, fee_parameters> &item : fee_map)
                  new_fees.parameters.insert(item.second);
            new_fees.scale = scale;
            new_fees.maximun_handling_fee = maximun_handling_fee;

            chain_parameters new_params = current_params;
            new_params.current_fees = new_fees;

            committee_member_update_global_parameters_operation update_op;
            update_op.new_parameters = new_params;

            proposal_create_operation prop_op;

            prop_op.expiration_time = expiration_time;
            prop_op.review_period_seconds = current_params.committee_proposal_review_period;
            prop_op.fee_paying_account = get_account(proposing_account).id;

            prop_op.proposed_ops.emplace_back(update_op);

            signed_transaction tx;
            tx.operations.push_back(prop_op);
            tx.validate();

            return sign_transaction(tx, broadcast);
      }

      signed_transaction approve_proposal(const string &fee_paying_account, const string &proposal_id, const approval_delta &delta, bool broadcast = false)
      {
            proposal_update_operation update_op;

            update_op.fee_paying_account = get_account(fee_paying_account).id;
            update_op.proposal = fc::variant(proposal_id).as<proposal_id_type>();
            // make sure the proposal exists
            get_object(update_op.proposal);

            for (const std::string &name : delta.active_approvals_to_add)
                  update_op.active_approvals_to_add.insert(get_account(name).id);
            for (const std::string &name : delta.active_approvals_to_remove)
                  update_op.active_approvals_to_remove.insert(get_account(name).id);
            for (const std::string &name : delta.owner_approvals_to_add)
                  update_op.owner_approvals_to_add.insert(get_account(name).id);
            for (const std::string &name : delta.owner_approvals_to_remove)
                  update_op.owner_approvals_to_remove.insert(get_account(name).id);
            for (const std::string &k : delta.key_approvals_to_add)
                  update_op.key_approvals_to_add.insert(public_key_type(k));
            for (const std::string &k : delta.key_approvals_to_remove)
                  update_op.key_approvals_to_remove.insert(public_key_type(k));

            signed_transaction tx;
            tx.operations.push_back(update_op);
            tx.validate();
            return sign_transaction(tx, broadcast);
      }

      void dbg_push_blocks(const std::string &src_filename, uint32_t count)
      {
            use_debug_api();
            (*_remote_debug)->debug_push_blocks(src_filename, count);
            (*_remote_debug)->debug_stream_json_objects_flush();
      }

      void dbg_generate_blocks(const std::string &debug_wif_key, uint32_t count)
      {
            use_debug_api();
            (*_remote_debug)->debug_generate_blocks(debug_wif_key, count);
            (*_remote_debug)->debug_stream_json_objects_flush();
      }

      void dbg_stream_json_objects(const std::string &filename)
      {
            use_debug_api();
            (*_remote_debug)->debug_stream_json_objects(filename);
            (*_remote_debug)->debug_stream_json_objects_flush();
      }

      void dbg_update_object(const fc::variant_object &update)
      {
            use_debug_api();
            (*_remote_debug)->debug_update_object(update);
            (*_remote_debug)->debug_stream_json_objects_flush();
      }

      void use_network_node_api()
      {
            if (_remote_net_node)
                  return;
            try
            {
                  _remote_net_node = _remote_api->network_node();
            }
            catch (const fc::exception &e)
            {
                  std::cerr << "\nCouldn't get network node API.  You probably are not configured\n"
                               "to access the network API on the witness_node you are\n"
                               "connecting to.  Please follow the instructions in README.md to set up an apiaccess file.\n"
                               "\n";
                  throw;
            }
      }

      void use_debug_api()
      {
            if (_remote_debug)
                  return;
            try
            {
                  _remote_debug = _remote_api->debug();
            }
            catch (const fc::exception &e)
            {
                  std::cerr << "\nCouldn't get debug node API.  You probably are not configured\n"
                               "to access the debug API on the node you are connecting to.\n"
                               "\n"
                               "To fix this problem:\n"
                               "- Please ensure you are running debug_node, not witness_node.\n"
                               "- Please follow the instructions in README.md to set up an apiaccess file.\n"
                               "\n";
            }
      }

      void network_add_nodes(const vector<string> &nodes)
      {
            use_network_node_api();
            for (const string &node_address : nodes)
            {
                  (*_remote_net_node)->add_node(fc::ip::endpoint::from_string(node_address));
            }
      }

      vector<variant> network_get_connected_peers()
      {
            use_network_node_api();
            const auto peers = (*_remote_net_node)->get_connected_peers();
            vector<variant> result;
            result.reserve(peers.size());
            for (const auto &peer : peers)
            {
                  variant v;
                  fc::to_variant(peer, v);
                  result.push_back(v);
            }
            return result;
      }

      operation get_prototype_operation_by_name(string operation_name)
      {
            auto it = _prototype_ops.find(operation_name);
            if (it == _prototype_ops.end())
                  FC_THROW("Unsupported operation: \"${operation_name}\"", ("operation_name", operation_name));
            return it->second;
      }
      operation get_prototype_operation_by_idx(uint idx)
      {
            return _remote_db->get_prototype_operation_by_idx(idx);
      }

      string _wallet_filename;
      wallet_data _wallet;

      map<public_key_type, string> _keys;
      fc::sha512 _checksum;

      chain_id_type _chain_id;
      fc::api<login_api> _remote_api;
      fc::api<database_api> _remote_db;
      fc::api<network_broadcast_api> _remote_net_broadcast;
      fc::api<history_api> _remote_hist;
      fc::optional<fc::api<network_node_api>> _remote_net_node;
      fc::optional<fc::api<graphene::debug_witness::debug_api>> _remote_debug;

      flat_map<string, operation> _prototype_ops;

      static_variant_map _operation_which_map = create_static_variant_map<operation>();

#ifdef __unix__
      mode_t _old_umask;
#endif
      const string _wallet_filename_extension = ".wallet";

      mutable map<asset_id_type, asset_object> _asset_cache;
};

std::string operation_printer::fee(const asset &a) const
{
      out << "   (Fee: " << wallet.get_asset(a.asset_id).amount_to_pretty_string(a) << ")";
      return "";
}

template <typename T>
std::string operation_printer::operator()(const T &op) const
{
      //balance_accumulator acc;
      //op.get_balance_delta( acc, result );
      auto payer = wallet.get_account(op.fee_payer());

      string op_name = fc::get_typename<T>::name();
      if (op_name.find_last_of(':') != string::npos)
            op_name.erase(0, op_name.find_last_of(':') + 1);
      out << op_name << " ";
      operation_result_printer rprinter(wallet);
      std::string str_result = result.visit(rprinter);
      if (str_result != "")
      {
            out << "   result: " << str_result;
      }
      return "";
}

std::string operation_printer::operator()(const account_create_operation &op) const
{
      out << "Create Account '" << op.name << "'";
      return "";
}

std::string operation_printer::operator()(const account_update_operation &op) const
{
      out << "Update Account '" << wallet.get_account(op.account).name << "'";
      return "";
}

std::string operation_printer::operator()(const asset_create_operation &op) const
{
      out << "Create ";
      if (op.bitasset_opts.valid())
            out << "BitAsset ";
      else
            out << "User-Issue Asset ";
      out << "'" << op.symbol << "' with issuer " << wallet.get_account(op.issuer).name;
      return "";
}

std::string operation_result_printer::operator()(const void_result &x) const
{
      return "";
}

std::string operation_result_printer::operator()(const object_id_result &oid)
{
      return std::string(oid.result);
}

std::string operation_result_printer::operator()(const asset_result &a)
{
      return _wallet.get_asset(a.result.asset_id).amount_to_pretty_string(a.result);
}
std::string operation_result_printer::operator()(const contract_result &a)
{
      return "contract result";
}
std::string operation_result_printer::operator()(const logger_result &err)
{
      return std::string(err.message);
}
std::string operation_result_printer::operator()(const error_result &err)
{
      return std::string(err.message);
}
} // namespace detail
} // namespace wallet
} // namespace graphene

namespace graphene
{
namespace wallet
{
vector<brain_key_info> utility::derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys)
{
      // Safety-check
      FC_ASSERT(number_of_desired_keys >= 1);

      // Create as many derived owner keys as requested
      vector<brain_key_info> results;
      brain_key = graphene::wallet::detail::normalize_brain_key(brain_key);
      for (int i = 0; i < number_of_desired_keys; ++i)
      {
            fc::ecc::private_key priv_key = graphene::wallet::detail::derive_private_key(brain_key, i);

            brain_key_info result;
            result.brain_priv_key = brain_key;
            result.wif_priv_key = key_to_wif(priv_key);
            result.pub_key = priv_key.get_public_key();

            results.push_back(result);
      }

      return results;
}
} // namespace wallet
} // namespace graphene

namespace graphene
{
namespace wallet
{

wallet_api::wallet_api(const wallet_data &initial_data, fc::api<login_api> rapi)
    : my(new detail::wallet_api_impl(*this, initial_data, rapi))
{
}

wallet_api::~wallet_api()
{
}

bool wallet_api::copy_wallet_file(string destination_filename)
{
      return my->copy_wallet_file(destination_filename);
}

fc::optional<signed_block> wallet_api::get_block(uint32_t num)
{
      return my->_remote_db->get_block(num);
}

uint64_t wallet_api::get_account_count() const
{
      return my->_remote_db->get_account_count();
}

vector<account_object> wallet_api::list_my_accounts()
{
      return vector<account_object>(my->_wallet.my_accounts.begin(), my->_wallet.my_accounts.end());
}

map<string, account_id_type> wallet_api::list_accounts(const string &lowerbound, uint32_t limit)
{
      return my->_remote_db->lookup_accounts(lowerbound, limit);
}

vector<asset> wallet_api::list_account_balances(const string &id)
{
      if (auto real_id = detail::maybe_id<account_id_type>(id))
            return my->_remote_db->get_account_balances(*real_id, flat_set<asset_id_type>());
      return my->_remote_db->get_account_balances(get_account(id).id, flat_set<asset_id_type>());
}

vector<asset_object> wallet_api::list_assets(const string &lowerbound, uint32_t limit) const
{
      return my->_remote_db->list_assets(lowerbound, limit);
}
vector<asset_restricted_object> wallet_api::list_asset_restricted_objects(const asset_id_type asset_id, restricted_enum restricted_type) const
{
      return my->_remote_db->list_asset_restricted_objects(asset_id, restricted_type);
}
vector<operation_detail> wallet_api::get_account_history(string name, int limit) const
{
      vector<operation_detail> result;
      auto account_id = get_account(name).get_id();

      while (limit > 0)
      {
            operation_history_id_type start;
            if (result.size())
            {
                  start = result.back().op.id;
                  start = start + 1;
            }

            vector<operation_history_object> current = my->_remote_hist->get_account_history(account_id, operation_history_id_type(), std::min(100, limit), start);
            for (auto &o : current)
            {
                  std::stringstream ss;
                  auto memo = o.op.visit(detail::operation_printer(ss, *my, o.result));
                  result.push_back(operation_detail{memo, ss.str(), o});
            }
            if (int(current.size()) < std::min(100, limit))
                  break;
            limit -= current.size();
      }

      return result;
}

vector<operation_detail> wallet_api::get_relative_account_history(string name, uint32_t stop, int limit, uint32_t start) const
{
      vector<operation_detail> result;
      auto account_id = get_account(name).get_id();

      const account_object &account = my->get_account(account_id);
      const account_statistics_object &stats = my->get_object(account.statistics);

      if (start == 0)
            start = stats.total_ops;
      else
            start = std::min<uint32_t>(start, stats.total_ops);

      while (limit > 0)
      {
            vector<operation_history_object> current = my->_remote_hist->get_relative_account_history(account_id, stop, std::min<uint32_t>(100, limit), start);
            for (auto &o : current)
            {
                  std::stringstream ss;
                  auto memo = o.op.visit(detail::operation_printer(ss, *my, o.result));
                  result.push_back(operation_detail{memo, ss.str(), o});
            }
            if (current.size() < std::min<uint32_t>(100, limit))
                  break;
            limit -= current.size();
            start -= 100;
            if (start == 0)
                  break;
      }
      return result;
}

vector<bucket_object> wallet_api::get_market_history(string symbol1, string symbol2, uint32_t bucket, fc::time_point_sec start, fc::time_point_sec end) const
{
      return my->_remote_hist->get_market_history(get_asset_id(symbol1), get_asset_id(symbol2), bucket, start, end);
}

vector<limit_order_object> wallet_api::get_limit_orders(string a, string b, uint32_t limit) const
{
      return my->_remote_db->get_limit_orders(get_asset(a).id, get_asset(b).id, limit);
}

vector<call_order_object> wallet_api::get_call_orders(string a, uint32_t limit) const
{
      return my->_remote_db->get_call_orders(get_asset(a).id, limit);
}

vector<force_settlement_object> wallet_api::get_settle_orders(string a, uint32_t limit) const
{
      return my->_remote_db->get_settle_orders(get_asset(a).id, limit);
}

vector<collateral_bid_object> wallet_api::get_collateral_bids(string asset, uint32_t limit, uint32_t start) const
{
      return my->_remote_db->get_collateral_bids(get_asset(asset).id, limit, start);
}

brain_key_info wallet_api::suggest_brain_key() const
{
      brain_key_info result;
      // create a private key for secure entropy
      fc::sha256 sha_entropy1 = fc::ecc::private_key::generate().get_secret();
      fc::sha256 sha_entropy2 = fc::ecc::private_key::generate().get_secret();
      fc::bigint entropy1(sha_entropy1.data(), sha_entropy1.data_size());
      fc::bigint entropy2(sha_entropy2.data(), sha_entropy2.data_size());
      fc::bigint entropy(entropy1);
      entropy <<= 8 * sha_entropy1.data_size();
      entropy += entropy2;
      string brain_key = "";

      for (int i = 0; i < BRAIN_KEY_WORD_COUNT; i++)
      {
            fc::bigint choice = entropy % graphene::words::word_list_size;
            entropy /= graphene::words::word_list_size;
            if (i > 0)
                  brain_key += " ";
            brain_key += graphene::words::word_list[choice.to_int64()];
      }

      brain_key = normalize_brain_key(brain_key);
      fc::ecc::private_key priv_key = derive_private_key(brain_key, 0);
      result.brain_priv_key = brain_key;
      result.wif_priv_key = key_to_wif(priv_key);
      result.address_info = priv_key.get_public_key();
      result.pub_key = priv_key.get_public_key();
      return result;
}

vector<brain_key_info> wallet_api::derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys) const
{
      return graphene::wallet::utility::derive_owner_keys_from_brain_key(brain_key, number_of_desired_keys);
}

bool wallet_api::is_public_key_registered(string public_key) const
{
      bool is_known = my->_remote_db->is_public_key_registered(public_key);
      return is_known;
}

string wallet_api::serialize_transaction(signed_transaction tx) const
{
      return fc::to_hex(fc::raw::pack(tx));
}

variant wallet_api::get_object(object_id_type id) const
{
      return my->_remote_db->get_objects({id});
}

string wallet_api::get_wallet_filename() const
{
      return my->get_wallet_filename();
}

transaction_handle_type wallet_api::begin_builder_transaction()
{
      return my->begin_builder_transaction();
}

void wallet_api::add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation &op)
{
      my->add_operation_to_builder_transaction(transaction_handle, op);
}

void wallet_api::replace_operation_in_builder_transaction(transaction_handle_type handle, unsigned operation_index, const operation &new_op)
{
      my->replace_operation_in_builder_transaction(handle, operation_index, new_op);
}

transaction wallet_api::preview_builder_transaction(transaction_handle_type handle)
{
      return my->preview_builder_transaction(handle);
}

pair<tx_hash_type, signed_transaction> wallet_api::sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast)
{
      auto tx = my->sign_builder_transaction(transaction_handle, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::propose_builder_transaction(
    transaction_handle_type handle,
    string account_name_or_id,
    time_point_sec expiration,
    uint32_t review_period_seconds,
    bool broadcast)
{
      auto tx = my->propose_builder_transaction(handle, account_name_or_id, expiration, review_period_seconds, broadcast);
      return std::make_pair(tx.hash(), tx);
}

void wallet_api::remove_builder_transaction(transaction_handle_type handle)
{
      return my->remove_builder_transaction(handle);
}

account_object wallet_api::get_account(string account_name_or_id) const
{
      return my->get_account(account_name_or_id);
}

asset_object wallet_api::get_asset(string asset_name_or_id) const
{
      auto a = my->find_asset(asset_name_or_id);
      FC_ASSERT(a);
      return *a;
}

asset_bitasset_data_object wallet_api::get_bitasset_data(string asset_name_or_id) const
{
      auto asset = get_asset(asset_name_or_id);
      FC_ASSERT(asset.is_market_issued() && asset.bitasset_data_id);
      return my->get_object<asset_bitasset_data_object>(*asset.bitasset_data_id);
}

account_id_type wallet_api::get_account_id(string account_name_or_id) const
{
      return my->get_account_id(account_name_or_id);
}

asset_id_type wallet_api::get_asset_id(string asset_symbol_or_id) const
{
      return my->get_asset_id(asset_symbol_or_id);
}

bool wallet_api::import_key(string account_name_or_id, string wif_key)
{
      FC_ASSERT(!is_locked());
      // backup wallet
      fc::optional<fc::ecc::private_key> optional_private_key = wif_to_key(wif_key);
      if (!optional_private_key)
            FC_THROW("Invalid private key");
      string shorthash = detail::address_to_shorthash(optional_private_key->get_public_key());
      copy_wallet_file("before-import-key-" + shorthash);

      if (my->import_key(account_name_or_id, wif_key))
      {
            save_wallet_file();
            copy_wallet_file("after-import-key-" + shorthash);
            return true;
      }
      return false;
}

string wallet_api::normalize_brain_key(string s) const
{
      return detail::normalize_brain_key(s);
}

variant wallet_api::info()
{
      return my->info();
}

variant_object wallet_api::about() const
{
      return my->about();
}

fc::ecc::private_key wallet_api::derive_private_key(const std::string &prefix_string, int sequence_number) const
{
      return detail::derive_private_key(prefix_string, sequence_number);
}

pair<tx_hash_type, signed_transaction> wallet_api::register_account(string name,
                                                                    public_key_type owner_pubkey,
                                                                    public_key_type active_pubkey,
                                                                    string registrar_account,
                                                                    bool broadcast)
{
      auto tx = my->register_account(name, owner_pubkey, active_pubkey, registrar_account, broadcast);
      return std::make_pair(tx.hash(), tx);
}
pair<tx_hash_type, signed_transaction> wallet_api::create_account_with_brain_key(string brain_key, string account_name,
                                                                                 string registrar_account,
                                                                                 bool broadcast /* = false */)
{
      auto tx = my->create_account_with_brain_key(
          brain_key, account_name, registrar_account, broadcast);
      return std::make_pair(tx.hash(), tx);
}
pair<tx_hash_type, signed_transaction> wallet_api::issue_asset(string to_account, string amount, string symbol,
                                                               pair<string, bool> memo, bool broadcast)
{
      auto tx = my->issue_asset(to_account, amount, symbol, memo, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::transfer(string from, string to, string amount,
                                                            string asset_symbol, pair<string, bool> memo, bool broadcast /* = false */)
{
      auto tx = my->transfer(from, to, amount, asset_symbol, memo, broadcast);
      return std::make_pair(tx.hash(), tx);
}
/*nico add*/
int wallet_api::add_extern_sign_key(string wif)
{
      return my->add_extern_sign_key(wif);
}
fc::optional<processed_transaction> wallet_api::get_transaction_by_id(string id)
{
      return my->_remote_db->get_transaction_by_id(id);
}
flat_set<public_key_type> wallet_api::get_signature_keys(const signed_transaction &trx)
{
      return my->_remote_db->get_signature_keys(trx);
}
fc::optional<transaction_in_block_info> wallet_api::get_transaction_in_block_info(const string &id)
{
      return my->_remote_db->get_transaction_in_block_info(id);
}
chain_property_object wallet_api::get_chain_properties()
{
      return my->_remote_db->get_chain_properties();
}
pair<tx_hash_type, signed_transaction> wallet_api::create_contract(string owner, string name, public_key_type contract_authority, string data, bool broadcast /* = false */)
{
      auto tx = my->create_contract(owner, name, contract_authority, data, broadcast);
      return std::make_pair(tx.hash(), tx);
}
pair<tx_hash_type, signed_transaction> wallet_api::revise_contract(string reviser, string contract_id_or_name, string data, bool broadcast /*= false*/)
{
      auto tx = my->revise_contract(reviser, contract_id_or_name, data, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::update_collateral_for_gas(account_id_type mortgager, account_id_type beneficiary, share_type collateral, bool broadcast)
{
      auto tx = my->update_collateral_for_gas(mortgager, beneficiary, collateral, broadcast);
      return std::make_pair(tx.hash(), tx);
}

fc::optional<contract_object> wallet_api::get_contract(string contract_id_or_name)
{
      return my->_remote_db->get_contract(contract_id_or_name);
}

fc::variant wallet_api::get_account_contract_data(const string &account_id, string contract_id_or_name)
{
      auto contract = my->_remote_db->get_contract(contract_id_or_name);
      if (auto real_id = detail::maybe_id<account_id_type>(account_id))
            return my->_remote_db->get_account_contract_data(*real_id, contract->id);
      return my->_remote_db->get_account_contract_data(get_account(account_id).id, contract->id);
}
lua_map wallet_api::get_contract_public_data(string contract_id_or_name, lua_map filter)
{
      return my->_remote_db->get_contract_public_data(contract_id_or_name, filter);
}
pair<tx_hash_type, signed_transaction> wallet_api::call_contract_function(string account_id_or_name, string contract_id_or_name, string function_name, vector<lua_types> value_list, bool broadcast /* = false */)
{
      auto tx = my->call_contract_function(account_id_or_name, contract_id_or_name, function_name, value_list, broadcast);
      return std::make_pair(tx.hash(), tx);
}
pair<tx_hash_type, signed_transaction> wallet_api::adjustment_temporary_authorization(string account_id_or_name, string describe, fc::time_point_sec expiration_time, flat_map<public_key_type, weight_type> temporary_active, bool broadcast /* = false */) //nico add :: wallet 合约 API
{
      auto tx = my->adjustment_temporary_authorization(account_id_or_name, describe, expiration_time, temporary_active, broadcast);
      return std::make_pair(tx.hash(), tx);
}
processed_transaction wallet_api::validate_transaction(transaction_handle_type transaction_handle)
{
      return my->_remote_db->validate_transaction(sign_builder_transaction(transaction_handle, false).second);
}
pair<tx_hash_type, signed_transaction> wallet_api::register_nh_asset_creator(
    const string &fee_paying_account,
    bool broadcast)
{
      auto tx = my->register_nh_asset_creator(fee_paying_account, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::create_world_view(
    const string &fee_paying_account,
    const string &world_view,
    bool broadcast)
{
      auto tx = my->create_world_view(fee_paying_account, world_view, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::propose_relate_world_view(
    const string &proposing_account,
    fc::time_point_sec expiration_time,
    const string &world_view_owner,
    const string &world_view,
    bool broadcast)
{
      auto tx = my->propose_relate_world_view(proposing_account, expiration_time, world_view_owner, world_view, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::create_nh_asset(
    const string &creator,
    const string &owner,
    const string &asset_id,
    const string &world_view,
    const string &base_describe,
    bool broadcast)
{
      auto tx = my->create_nh_asset(creator, owner, asset_id, world_view, base_describe, broadcast);
      return std::make_pair(tx.hash(), tx);
}

std::pair<vector<nh_asset_object>, uint32_t> wallet_api::list_nh_asset_by_creator(
    const string &nh_asset_creator, const string &world_view,
    uint32_t pagesize,
    uint32_t page)
{
      return (my->_remote_db->list_nh_asset_by_creator(get_account(nh_asset_creator).id, world_view, pagesize, page));
}

std::pair<vector<nh_asset_object>, uint32_t> wallet_api::list_account_nh_asset(
    const string &nh_asset_owner,
    const vector<string> &world_view_name_or_ids,
    uint32_t pagesize,
    uint32_t page,
    nh_asset_list_type list_type)
{
      return my->_remote_db->list_account_nh_asset(get_account(nh_asset_owner).id, world_view_name_or_ids, pagesize, page, list_type);
}

pair<tx_hash_type, signed_transaction> wallet_api::transfer_nh_asset(
    const string &from,
    const string &to,
    const string &nh_asset,
    bool broadcast)
{
      auto tx = my->transfer_nh_asset(from, to, nh_asset, broadcast);
      return std::make_pair(tx.hash(), tx);
}

fc::optional<nh_asset_creator_object> wallet_api::get_nh_creator(const string &nh_asset_creator)
{
      return my->_remote_db->get_nh_creator(get_account(nh_asset_creator).id);
}
/*
pair<tx_hash_type, signed_transaction> wallet_api::relate_nh_asset(
    const string &nh_asset_creator,
    const string &parent,
    const string &child,
    const string &contract_id_or_name,
    bool relate,
    bool broadcast)
{
      auto tx = my->relate_nh_asset(nh_asset_creator, parent, child, contract_id_or_name, relate, broadcast);
      return std::make_pair(tx.hash(), tx);
}
*/
pair<tx_hash_type, signed_transaction> wallet_api::delete_nh_asset(
    const string &fee_paying_account,
    const string &nh_asset,
    bool broadcast)

{
      auto tx = my->delete_nh_asset(fee_paying_account, nh_asset, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::create_nh_asset_order(
    const string &seller,
    const string &otcaccount,
    const string &pending_fee_amount,
    const string &pending_fee_symbol,
    const string &nh_asset,
    const string &price_amount,
    const string &price_symbol,
    const string &memo,
    fc::time_point_sec expiration_time,
    bool broadcast)
{
      auto tx = my->create_nh_asset_order(seller, otcaccount, pending_fee_amount, pending_fee_symbol, nh_asset, price_amount, price_symbol, memo, expiration_time, broadcast);
      return std::make_pair(tx.hash(), tx);
}

std::pair<vector<nh_asset_order_object>, uint32_t> wallet_api::list_nh_asset_order(
    const string &asset_symbols_or_id,
    const string &world_view_name_or_id,
    const string &base_describe,
    uint32_t pagesize,
    uint32_t page,
    bool is_ascending_order)
{
      return my->_remote_db->list_nh_asset_order(asset_symbols_or_id, world_view_name_or_id, base_describe, pagesize, page, is_ascending_order);
}

pair<tx_hash_type, signed_transaction> wallet_api::cancel_nh_asset_order(object_id_type order_id, bool broadcast)
{
      auto tx = my->cancel_nh_asset_order(order_id, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::fill_nh_asset_order(
    const string &fee_paying_account,
    nh_asset_order_id_type order_id,
    bool broadcast)
{
      auto tx = my->fill_nh_asset_order(fee_paying_account, order_id, broadcast);
      return std::make_pair(tx.hash(), tx);
}

// List the nht orders submitted under the account
std::pair<vector<nh_asset_order_object>, uint32_t> wallet_api::list_account_nh_asset_order(
    const string &nh_asset_order_owner,
    uint32_t pagesize,
    uint32_t page)
{
      return my->_remote_db->list_account_nh_asset_order(get_account(nh_asset_order_owner).id, pagesize, page);
}
signed_transaction wallet_api::create_file(
    const string &file_creator,
    const string &file_name,
    const string &file_content,
    bool broadcast)
{
      return my->create_file(file_creator, file_name, file_content, broadcast);
}

signed_transaction wallet_api::add_file_relate_account(
    const string &file_creator,
    const string &file_name_or_id,
    const vector<string> &related_account,
    bool broadcast)
{
      return my->add_file_relate_account(file_creator, file_name_or_id, related_account, broadcast);
}

signed_transaction wallet_api::file_signature(
    const string &signature_account,
    const string &file_name_or_id,
    const string &signature,
    bool broadcast)
{
      return my->file_signature(signature_account, file_name_or_id, signature, broadcast);
}

signed_transaction wallet_api::propose_relate_parent_file(
    const string &file_creator,
    const string &parent_file_name_or_id,
    const string &sub_file_name_or_id,
    fc::time_point_sec expiration_time,
    bool broadcast)
{
      return my->propose_relate_parent_file(file_creator, parent_file_name_or_id, sub_file_name_or_id, expiration_time, broadcast);
}

fc::optional<file_object> wallet_api::lookup_file(const string &file_name_or_ids) const
{
      return my->_remote_db->lookup_file(file_name_or_ids);
}

map<string, file_id_type> wallet_api::list_account_created_file(const string &file_creator) const
{
      return my->_remote_db->list_account_created_file(get_account(file_creator).id);
}
pair<pair<tx_hash_type, pair<object_id_type, account_transaction_history_id_type>>, processed_transaction> wallet_api::get_account_transaction(const string account_id_or_name, account_transaction_history_id_type transaction_history_id)
{
      account_transaction_history_object transaction_history_object;
      auto account = my->get_account(account_id_or_name);
      transaction_history_object = my->get_object(transaction_history_id);
      FC_ASSERT(transaction_history_object.account == account.id);
      auto processed_op = my->get_object(transaction_history_object.operation_id);
      auto processed_rx = my->_remote_db->get_transaction(processed_op.block_num, processed_op.trx_in_block);
      return std::make_pair(std::make_pair(processed_rx.hash(), std::make_pair(transaction_history_object.id, transaction_history_object.next)), processed_rx);
}
pair<pair<tx_hash_type, pair<object_id_type, account_transaction_history_id_type>>, processed_transaction> wallet_api::get_account_top_transaction(const string account_id_or_name)
{
      auto account = my->get_account(account_id_or_name);
      auto statistics_object = my->get_object(account.statistics);
      auto transaction_history_id = statistics_object.most_recent_op;
      auto transaction_history_object = my->get_object(transaction_history_id);
      auto processed_op = my->get_object(transaction_history_object.operation_id);
      auto processed_rx = my->_remote_db->get_transaction(processed_op.block_num, processed_op.trx_in_block);
      return std::make_pair(std::make_pair(processed_rx.hash(), std::make_pair(transaction_history_object.id, transaction_history_object.next)), processed_rx);
}
// crontab start
pair<tx_hash_type, signed_transaction> wallet_api::create_crontab(
    const string &task_creator,
    fc::time_point_sec start_time,
    uint16_t execute_interval,
    uint64_t target_execute_times,
    const vector<operation> &crontab_ops,
    bool broadcast)
{
      auto tx = my->create_crontab(task_creator, start_time, execute_interval, target_execute_times, crontab_ops, broadcast);
      return std::make_pair(tx.hash(), tx);
}
pair<tx_hash_type, signed_transaction> wallet_api::crontab_builder_transaction(transaction_handle_type handle, const string &task_creator,
                                                                               fc::time_point_sec start_time, uint16_t execute_interval, uint64_t target_execute_times, bool broadcast)
{
      auto tx = my->crontab_builder_transaction(handle, task_creator, start_time, execute_interval, target_execute_times, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::cancel_crontab(
    const string &task_creator,
    object_id_type task_id,
    bool broadcast)
{
      auto tx = my->cancel_crontab(task_creator, task_id, broadcast);
      return std::make_pair(tx.hash(), tx);
}

vector<crontab_object> wallet_api::list_account_crontab(const string &crontab_creator, bool contain_normal, bool contain_suspended)
{
      return my->_remote_db->list_account_crontab(get_account(crontab_creator).id, contain_normal, contain_suspended);
}

pair<tx_hash_type, signed_transaction> wallet_api::recover_crontab(const string &crontab_creator, object_id_type crontab_id, fc::time_point_sec restart_time, bool broadcast)
{
      auto tx = my->recover_crontab(crontab_creator, crontab_id, restart_time, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::asset_update_restricted_list(const string &asset_issuer, string target_asset, restricted_enum restricted_type, vector<object_id_type> restricted_list, bool isadd, bool broadcast)
{
      auto tx = my->asset_update_restricted_list(asset_issuer, target_asset, restricted_type, restricted_list, isadd, broadcast);
      return std::make_pair(tx.hash(), tx);
}

// crontab end

/*nico end*/

pair<tx_hash_type, signed_transaction> wallet_api::create_asset(string issuer,
                                                                string symbol,
                                                                uint8_t precision,
                                                                asset_options common,
                                                                fc::optional<bitasset_options> bitasset_opts,
                                                                bool broadcast)

{
      auto tx = my->create_asset(issuer, symbol, precision, common, bitasset_opts, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::update_asset(string symbol,
                                                                fc::optional<string> new_issuer,
                                                                asset_options new_options,
                                                                bool broadcast /* = false */)
{
      auto tx = my->update_asset(symbol, new_issuer, new_options, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::update_bitasset(string symbol,
                                                                   bitasset_options new_options,
                                                                   bool broadcast /* = false */)
{
      auto tx = my->update_bitasset(symbol, new_options, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::update_asset_feed_producers(string symbol,
                                                                               flat_set<string> new_feed_producers,
                                                                               bool broadcast /* = false */)
{
      auto tx = my->update_asset_feed_producers(symbol, new_feed_producers, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::publish_asset_feed(string publishing_account,
                                                                      string symbol,
                                                                      price_feed feed,
                                                                      bool broadcast /* = false */)
{
      auto tx = my->publish_asset_feed(publishing_account, symbol, feed, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::reserve_asset(string from,
                                                                 string amount,
                                                                 string symbol,
                                                                 bool broadcast /* = false */)
{
      auto tx = my->reserve_asset(from, amount, symbol, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::global_settle_asset(string symbol,
                                                                       price settle_price,
                                                                       bool broadcast /* = false */)
{
      auto tx = my->global_settle_asset(symbol, settle_price, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::settle_asset(string account_to_settle,
                                                                string amount_to_settle,
                                                                string symbol,
                                                                bool broadcast /* = false */)
{
      auto tx = my->settle_asset(account_to_settle, amount_to_settle, symbol, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::bid_collateral(string bidder_name,
                                                                  string debt_amount, string debt_symbol,
                                                                  string additional_collateral,
                                                                  bool broadcast)
{
      auto tx = my->bid_collateral(bidder_name, debt_amount, debt_symbol, additional_collateral, broadcast);
      return std::make_pair(tx.hash(), tx);
}

/*
pair<tx_hash_type, signed_transaction> wallet_api::whitelist_account(string authorizing_account,
                                                                     string account_to_list,
                                                                     account_whitelist_operation::account_listing new_listing_status,
                                                                     bool broadcast )
{
      auto tx = my->whitelist_account(authorizing_account, account_to_list, new_listing_status, broadcast);
      return std::make_pair(tx.hash(), tx);
}
*/

pair<tx_hash_type, signed_transaction> wallet_api::create_committee_member(string owner_account, string url,
                                                                           bool broadcast /* = false */)
{
      auto tx = my->create_committee_member(owner_account, url, broadcast);
      return std::make_pair(tx.hash(), tx);
}
pair<tx_hash_type, signed_transaction> wallet_api::update_committee_member(string committee_account, string url, bool work_status,
                                                                           bool broadcast /* = false */)
{
      auto tx = my->update_committee_member(committee_account, url, work_status, broadcast);
      return std::make_pair(tx.hash(), tx);
}
map<string, witness_id_type> wallet_api::list_witnesses(const string &lowerbound, uint32_t limit)
{
      return my->_remote_db->lookup_witness_accounts(lowerbound, limit);
}

map<string, committee_member_id_type> wallet_api::list_committee_members(const string &lowerbound, uint32_t limit)
{
      return my->_remote_db->lookup_committee_member_accounts(lowerbound, limit);
}

witness_object wallet_api::get_witness(string owner_account)
{
      return my->get_witness(owner_account);
}

committee_member_object wallet_api::get_committee_member(string owner_account)
{
      return my->get_committee_member(owner_account);
}

pair<tx_hash_type, signed_transaction> wallet_api::create_witness(string owner_account,
                                                                  string url,
                                                                  bool broadcast /* = false */)
{
      auto tx = my->create_witness(owner_account, url, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::update_witness(
    string witness_name,
    string url,
    string block_signing_key,
    bool work_status,
    bool broadcast /* = false */)
{
      auto tx = my->update_witness(witness_name, url, block_signing_key, work_status, broadcast);
      return std::make_pair(tx.hash(), tx);
}

vector<vesting_balance_object_with_info> wallet_api::get_vesting_balances(string account_name)
{
      return my->get_vesting_balances(account_name);
}

pair<tx_hash_type, signed_transaction> wallet_api::withdraw_vesting(
    string witness_name,
    string amount,
    string asset_symbol,
    bool broadcast /* = false */)
{
      auto tx = my->withdraw_vesting(witness_name, amount, asset_symbol, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::vote_for_committee_member(string voting_account,
                                                                             string witness,
                                                                             int64_t approve,
                                                                             bool broadcast /* = false */)
{
      auto tx = my->vote_for_committee_member(voting_account, witness, approve, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::vote_for_witness(string voting_account,
                                                                    string witness,
                                                                    int64_t approve,
                                                                    bool broadcast /* = false */)
{
      auto tx = my->vote_for_witness(voting_account, witness, approve, broadcast);
      return std::make_pair(tx.hash(), tx);
}

void wallet_api::set_wallet_filename(string wallet_filename)
{
      my->_wallet_filename = wallet_filename;
}

pair<tx_hash_type, signed_transaction> wallet_api::sign_transaction(signed_transaction tx, bool broadcast /* = false */)
{
      try
      {
            auto tx1 = my->sign_transaction(tx, broadcast);
            return std::make_pair(tx1.hash(), tx1);
      }
      FC_CAPTURE_AND_RETHROW((tx))
}

operation wallet_api::get_prototype_operation_by_name(string operation_name)
{
      return my->get_prototype_operation_by_name(operation_name);
}
operation wallet_api::get_prototype_operation_by_idx(uint index)
{
      return my->get_prototype_operation_by_idx(index);
}

void wallet_api::dbg_push_blocks(std::string src_filename, uint32_t count)
{
      my->dbg_push_blocks(src_filename, count);
}

void wallet_api::dbg_generate_blocks(std::string debug_wif_key, uint32_t count)
{
      my->dbg_generate_blocks(debug_wif_key, count);
}

void wallet_api::dbg_stream_json_objects(const std::string &filename)
{
      my->dbg_stream_json_objects(filename);
}

void wallet_api::dbg_update_object(fc::variant_object update)
{
      my->dbg_update_object(update);
}

void wallet_api::network_add_nodes(const vector<string> &nodes)
{
      my->network_add_nodes(nodes);
}

vector<variant> wallet_api::network_get_connected_peers()
{
      return my->network_get_connected_peers();
}

bool wallet_api::set_node_message_send_cache_size(const uint32_t &params)
{
      FC_ASSERT(!is_locked());
      my->use_network_node_api();
      (*(my->_remote_net_node))->set_message_send_cache_size(params);
      return true;
}

bool wallet_api::set_node_deduce_in_verification_mode(const bool &params)
{
      FC_ASSERT(!is_locked());
      my->use_network_node_api();
      (*(my->_remote_net_node))->set_deduce_in_verification_mode(params);
      return true;
}
/* void wallet_api::node_flush()
{
      FC_ASSERT(!is_locked());
      my->use_network_node_api();
      (*(my->_remote_net_node))->node_flush();
}*/

pair<tx_hash_type, signed_transaction> wallet_api::propose_parameter_change(
    const string &proposing_account,
    fc::time_point_sec expiration_time,
    const variant_object &changed_values,
    fc::optional<proposal_id_type> proposal_base,
    bool broadcast /* = false */
)
{
      auto tx = my->propose_parameter_change(proposing_account, expiration_time, changed_values, proposal_base, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::propose_fee_change(
    const string &proposing_account,
    fc::time_point_sec expiration_time,
    const variant_object &changed_fees,
    fc::optional<proposal_id_type> proposal_base,
    bool broadcast /* = false */
)
{
      auto tx = my->propose_fee_change(proposing_account, expiration_time, changed_fees, proposal_base, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::approve_proposal(
    const string &fee_paying_account,
    const string &proposal_id,
    const approval_delta &delta,
    bool broadcast /* = false */
)
{
      auto tx = my->approve_proposal(fee_paying_account, proposal_id, delta, broadcast);
      return std::make_pair(tx.hash(), tx);
}

global_property_object wallet_api::get_global_properties() const
{
      return my->get_global_properties();
}

dynamic_global_property_object wallet_api::get_dynamic_global_properties() const
{
      return my->get_dynamic_global_properties();
}

string wallet_api::help() const
{
      std::vector<std::string> method_names = my->method_documentation.get_method_names();
      std::stringstream ss;
      for (const std::string method_name : method_names)
      {
            try
            {
                  ss << my->method_documentation.get_brief_description(method_name);
            }
            catch (const fc::key_not_found_exception &)
            {
                  ss << method_name << " (no help available)\n";
            }
      }
      return ss.str();
}

string wallet_api::gethelp(const string &method) const
{
      fc::api<wallet_api> tmp;
      std::stringstream ss;
      ss << "\n";

      if (method == "import_key")
      {
            ss << "usage: import_key ACCOUNT_NAME_OR_ID  WIF_PRIVATE_KEY\n\n";
            ss << "example: import_key \"1.3.11\" 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3\n";
            ss << "example: import_key \"usera\" 5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3\n";
      }
      else if (method == "transfer")
      {
            ss << "usage: transfer FROM TO AMOUNT SYMBOL \"memo\" BROADCAST\n\n";
            ss << "example: transfer \"1.3.11\" \"1.3.4\" 1000.03 CORE \"memo\" true\n";
            ss << "example: transfer \"usera\" \"userb\" 1000.123 CORE \"memo\" true\n";
      }
      else if (method == "create_account_with_brain_key")
      {
            ss << "usage: create_account_with_brain_key BRAIN_KEY ACCOUNT_NAME REGISTRAR  BROADCAST\n\n";
            ss << "example: create_account_with_brain_key \"my really long brain key\" \"newaccount\" \"1.3.11\" \"1.3.11\" true\n";
            ss << "example: create_account_with_brain_key \"my really long brain key\" \"newaccount\" \"someaccount\" \"otheraccount\" true\n";
            ss << "\n";
            ss << "This method should be used if you would like the wallet to generate new keys derived from the brain key.\n";
            ss << "The BRAIN_KEY will be used as the owner key, and the active key will be derived from the BRAIN_KEY.  Use\n";
            ss << "register_account if you already know the keys you know the public keys that you would like to register.\n";
      }
      else if (method == "register_account")
      {
            ss << "usage: register_account ACCOUNT_NAME OWNER_PUBLIC_KEY ACTIVE_PUBLIC_KEY REGISTRAR   BROADCAST\n\n";
            ss << "example: register_account \"newaccount\" \"CORE6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\" \"CORE6MRyAjQq8ud7hVNYcfnVPJqcVpscN5So8BhtHuGYqET5GDW5CV\" \"1.3.11\" \"1.3.11\" 50 true\n";
            ss << "\n";
            ss << "Use this method to register an account for which you do not know the private keys.";
      }
      else if (method == "create_asset")
      {
            ss << "usage: ISSUER SYMBOL PRECISION_DIGITS OPTIONS BITASSET_OPTIONS BROADCAST\n\n";
            ss << "PRECISION_DIGITS: the number of digits after the decimal point\n\n";
            ss << "Example value of OPTIONS: \n";
            ss << fc::json::to_pretty_string(graphene::chain::asset_options());
            ss << "\nExample value of BITASSET_OPTIONS: \n";
            ss << fc::json::to_pretty_string(graphene::chain::bitasset_options());
            ss << "\nBITASSET_OPTIONS may be null\n";
      }
      else
      {
            std::string doxygenHelpString = my->method_documentation.get_detailed_description(method);
            if (!doxygenHelpString.empty())
                  ss << doxygenHelpString;
            else
                  ss << "No help defined for method " << method << "\n";
      }

      return ss.str();
}

bool wallet_api::load_wallet_file(string wallet_filename)
{
      return my->load_wallet_file(wallet_filename);
}

void wallet_api::save_wallet_file(string wallet_filename)
{
      my->save_wallet_file(wallet_filename);
}

std::map<string, std::function<string(fc::variant, const fc::variants &)>>
wallet_api::get_result_formatters() const
{
      return my->get_result_formatters();
}

bool wallet_api::is_locked() const
{
      return my->is_locked();
}
bool wallet_api::is_new() const
{
      return my->_wallet.cipher_keys.size() == 0;
}

void wallet_api::encrypt_keys()
{
      my->encrypt_keys();
}

void wallet_api::lock()
{
      try
      {
            FC_ASSERT(!is_locked());
            encrypt_keys();
            for (auto key : my->_keys)
                  key.second = key_to_wif(fc::ecc::private_key());
            my->_keys.clear();
            my->_checksum = fc::sha512();
            my->self.lock_changed(true);
      }
      FC_CAPTURE_AND_RETHROW()
}

void wallet_api::unlock(string password)
{
      try
      {
            FC_ASSERT(password.size() > 0);
            auto pw = fc::sha512::hash(password.c_str(), password.size());
            vector<char> decrypted = fc::aes_decrypt(pw, my->_wallet.cipher_keys);
            auto pk = fc::raw::unpack<plain_keys>(decrypted);
            FC_ASSERT(pk.checksum == pw);
            my->_keys = std::move(pk.keys);
            my->_checksum = pk.checksum;
            my->self.lock_changed(false);
      }
      FC_CAPTURE_AND_RETHROW()
}

void wallet_api::set_password(string password)
{
      if (!is_new())
            FC_ASSERT(!is_locked(), "The wallet must be unlocked before the password can be set");
      my->_checksum = fc::sha512::hash(password.c_str(), password.size());
      lock();
}

vector<signed_transaction> wallet_api::import_balance(string name_or_id, const vector<string> &wif_keys, bool broadcast)
{
      return my->import_balance(name_or_id, wif_keys, broadcast);
}

namespace detail
{

vector<signed_transaction> wallet_api_impl::import_balance(string name_or_id, const vector<string> &wif_keys, bool broadcast)
{
      try
      {
            FC_ASSERT(!is_locked());
            const dynamic_global_property_object &dpo = _remote_db->get_dynamic_global_properties();
            account_object claimer = get_account(name_or_id);
            uint32_t max_ops_per_tx = 30;

            map<address, private_key_type> keys; // local index of address -> private key
            vector<address> addrs;
            bool has_wildcard = false;
            addrs.reserve(wif_keys.size());
            for (const string &wif_key : wif_keys)
            {
                  if (wif_key == "*")
                  {
                        if (has_wildcard)
                              continue;
                        for (const public_key_type &pub : _wallet.extra_keys[claimer.id])
                        {
                              addrs.push_back(pub);
                              auto it = _keys.find(pub);
                              if (it != _keys.end())
                              {
                                    fc::optional<fc::ecc::private_key> privkey = wif_to_key(it->second);
                                    FC_ASSERT(privkey);
                                    keys[addrs.back()] = *privkey;
                              }
                              else
                              {
                                    wlog("Somehow _keys has no private key for extra_keys public key ${k}", ("k", pub));
                              }
                        }
                        has_wildcard = true;
                  }
                  else
                  {
                        fc::optional<private_key_type> key = wif_to_key(wif_key);
                        FC_ASSERT(key.valid(), "Invalid private key");
                        fc::ecc::public_key pk = key->get_public_key();
                        addrs.push_back(pk);
                        keys[addrs.back()] = *key;
                        // see chain/balance_evaluator.cpp
                        addrs.push_back(pts_address(pk, false, 56));
                        keys[addrs.back()] = *key;
                        addrs.push_back(pts_address(pk, true, 56));
                        keys[addrs.back()] = *key;
                        addrs.push_back(pts_address(pk, false, 0));
                        keys[addrs.back()] = *key;
                        addrs.push_back(pts_address(pk, true, 0));
                        keys[addrs.back()] = *key;
                  }
            }

            vector<balance_object> balances = _remote_db->get_balance_objects(addrs);
            wdump((balances));
            addrs.clear();

            set<asset_id_type> bal_types;
            for (auto b : balances)
                  bal_types.insert(b.balance.asset_id);

            struct claim_tx
            {
                  vector<balance_claim_operation> ops;
                  set<address> addrs;
            };
            vector<claim_tx> claim_txs;

            for (const asset_id_type &a : bal_types)
            {
                  balance_claim_operation op;
                  op.deposit_to_account = claimer.id;
                  for (const balance_object &b : balances)
                  {
                        if (b.balance.asset_id == a)
                        {
                              op.total_claimed = b.available(dpo.time);
                              if (op.total_claimed.amount == 0)
                                    continue;
                              op.balance_to_claim = b.id;
                              op.balance_owner_key = keys[b.owner].get_public_key();
                              if ((claim_txs.empty()) || (claim_txs.back().ops.size() >= max_ops_per_tx))
                                    claim_txs.emplace_back();
                              claim_txs.back().ops.push_back(op);
                              claim_txs.back().addrs.insert(b.owner);
                        }
                  }
            }

            vector<signed_transaction> result;

            for (const claim_tx &ctx : claim_txs)
            {
                  signed_transaction tx;
                  tx.operations.reserve(ctx.ops.size());
                  for (const balance_claim_operation &op : ctx.ops)
                        tx.operations.emplace_back(op);
                  tx.validate();
                  signed_transaction signed_tx = sign_transaction(tx, false);
                  for (const address &addr : ctx.addrs)
                        signed_tx.sign(keys[addr], _chain_id);
                  // if the key for a balance object was the same as a key for the account we're importing it into,
                  // we may end up with duplicate signatures, so remove those
                  boost::erase(signed_tx.signatures, boost::unique<boost::return_found_end>(boost::sort(signed_tx.signatures)));
                  result.push_back(signed_tx);
                  if (broadcast)
                        _remote_net_broadcast->broadcast_transaction(signed_tx);
            }

            return result;
      }
      FC_CAPTURE_AND_RETHROW((name_or_id))
}

} // namespace detail

map<public_key_type, string> wallet_api::dump_private_keys()
{
      FC_ASSERT(!is_locked());
      return my->_keys;
}

pair<tx_hash_type, signed_transaction> wallet_api::upgrade_account(string name, bool broadcast)
{
      auto tx = my->upgrade_account(name, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::sell_asset(string seller_account,
                                                              string amount_to_sell,
                                                              string symbol_to_sell,
                                                              string min_to_receive,
                                                              string symbol_to_receive,
                                                              uint32_t expiration,
                                                              bool fill_or_kill,
                                                              bool broadcast)
{
      auto tx = my->sell_asset(seller_account, amount_to_sell, symbol_to_sell, min_to_receive,
                               symbol_to_receive, expiration, fill_or_kill, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::sell(string seller_account,
                                                        string base,
                                                        string quote,
                                                        double rate,
                                                        double amount,
                                                        bool broadcast)
{
      auto tx = my->sell_asset(seller_account, std::to_string(amount), base,
                               std::to_string(rate * amount), quote, 0, false, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::buy(string buyer_account,
                                                       string base,
                                                       string quote,
                                                       double rate,
                                                       double amount,
                                                       bool broadcast)
{
      auto tx = my->sell_asset(buyer_account, std::to_string(rate * amount), quote,
                               std::to_string(amount), base, 0, false, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::borrow_asset(string seller_name, string amount_to_sell,
                                                                string asset_symbol, string amount_of_collateral, bool broadcast)
{
      FC_ASSERT(!is_locked());
      auto tx = my->borrow_asset(seller_name, amount_to_sell, asset_symbol, amount_of_collateral, broadcast);
      return std::make_pair(tx.hash(), tx);
}

pair<tx_hash_type, signed_transaction> wallet_api::cancel_order(object_id_type order_id, bool broadcast)
{
      FC_ASSERT(!is_locked());
      auto tx = my->cancel_order(order_id, broadcast);
      return std::make_pair(tx.hash(), tx);
}

memo_data wallet_api::sign_memo(string from, string to, string memo)
{
      FC_ASSERT(!is_locked());
      return my->sign_memo(from, to, memo);
}

string wallet_api::read_memo(const memo_data &memo)
{
      FC_ASSERT(!is_locked());
      return my->read_memo(memo);
}

string wallet_api::get_key_label(public_key_type key) const
{
      auto key_itr = my->_wallet.labeled_keys.get<by_key>().find(key);
      if (key_itr != my->_wallet.labeled_keys.get<by_key>().end())
            return key_itr->label;
      return string();
}

string wallet_api::get_private_key(public_key_type pubkey) const
{
      return key_to_wif(my->get_private_key(pubkey));
}

public_key_type wallet_api::get_public_key(string label) const
{
      try
      {
            return fc::variant(label).as<public_key_type>();
      }
      catch (...)
      {
      }

      auto key_itr = my->_wallet.labeled_keys.get<by_label>().find(label);
      if (key_itr != my->_wallet.labeled_keys.get<by_label>().end())
            return key_itr->key;
      return public_key_type();
}

bool wallet_api::set_key_label(public_key_type key, string label)
{
      auto result = my->_wallet.labeled_keys.insert(key_label{label, key});
      if (result.second)
            return true;

      auto key_itr = my->_wallet.labeled_keys.get<by_key>().find(key);
      auto label_itr = my->_wallet.labeled_keys.get<by_label>().find(label);
      if (label_itr == my->_wallet.labeled_keys.get<by_label>().end())
      {
            if (key_itr != my->_wallet.labeled_keys.get<by_key>().end())
                  return my->_wallet.labeled_keys.get<by_key>().modify(key_itr, [&](key_label &obj) { obj.label = label; });
      }
      return false;
}
order_book wallet_api::get_order_book(const string &base, const string &quote, unsigned limit)
{
      return (my->_remote_db->get_order_book(base, quote, limit));
}

vesting_balance_object_with_info::vesting_balance_object_with_info(const vesting_balance_object &vbo, fc::time_point_sec now)
    : vesting_balance_object(vbo)
{
      allowed_withdraw = get_allowed_withdraw(now);
      allowed_withdraw_time = now;
}

} // namespace wallet
} // namespace graphene

void fc::to_variant(const account_multi_index_type &accts, fc::variant &vo)
{
      vo = vector<account_object>(accts.begin(), accts.end());
}

void fc::from_variant(const fc::variant &var, account_multi_index_type &vo)
{
      const vector<account_object> &v = var.as<vector<account_object>>();
      vo = account_multi_index_type(v.begin(), v.end());
}
