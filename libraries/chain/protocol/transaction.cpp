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
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
#include <fc/io/raw.hpp>
#include <fc/bitutil.hpp>
#include <fc/smart_ref_impl.hpp>
#include <algorithm>
#include <graphene/chain/database.hpp>

namespace graphene
{
namespace chain
{

digest_type processed_transaction::merkle_digest() const
{
      digest_type::encoder enc;
      fc::raw::pack(enc, *this);
      return enc.result();
}

void processed_transaction::pack(digest_type::encoder &enc) const
{
      fc::raw::pack(enc, *this);
}

digest_type transaction::digest() const
{
      digest_type::encoder enc;
      fc::raw::pack(enc, *this);
      return enc.result();
}

digest_type transaction::sig_digest(const chain_id_type &chain_id) const
{
      digest_type::encoder enc;
      fc::raw::pack(enc, chain_id);
      fc::raw::pack(enc, *this);
      return enc.result();
}

void transaction::validate() const // nico validate 验证
{
      FC_ASSERT(operations.size() > 0 && operations.size() <= 1000, "A transaction must have at least one operation and should not exceed 1000 at most.", ("trx", *this));
      for (const auto &op : operations)
      {     
            FC_ASSERT(op.which()!=operation::tag<fill_order_operation>::value&&
                      op.which()!=operation::tag<asset_settle_cancel_operation>::value&&
                      op.which()!=operation::tag<execute_bid_operation>::value&&
                      op.which()!=operation::tag<account_authentication_operation>::value);
            operation_validate(op);
      }
}

graphene::chain::transaction_id_type graphene::chain::transaction::id() const
{
      auto h = this->digest();
      return id(h);
}

graphene::chain::transaction_id_type graphene::chain::transaction::id(digest_type hash_value) const
{
      transaction_id_type result;
      memcpy(result._hash, hash_value._hash, std::min(sizeof(result), sizeof(hash_value)));
      return result;
      //return transaction_id_type::hash(hash_value);
}

const signature_type &graphene::chain::signed_transaction::sign(const private_key_type &key, const chain_id_type &chain_id)
{
      digest_type h = sig_digest(chain_id);
      signatures.push_back(key.sign_compact(h));
      return signatures.back();
}

signature_type graphene::chain::signed_transaction::sign(const private_key_type &key, const chain_id_type &chain_id) const
{
      digest_type::encoder enc;
      fc::raw::pack(enc, chain_id);
      fc::raw::pack(enc, *this);
      return key.sign_compact(enc.result());
}

void transaction::set_expiration(fc::time_point_sec expiration_time)
{
      expiration = expiration_time;
}

void transaction::set_reference_block(const block_id_type &reference_block)
{
      ref_block_num = fc::endian_reverse_u32(reference_block._hash[0]);
      ref_block_prefix = reference_block._hash[1];
}

void transaction::get_required_authorities(flat_set<account_id_type> &active, flat_set<account_id_type> &owner, vector<authority> &other) const
{
      for (const auto &op : operations)
            operation_get_required_authorities(op, active, owner, other);
}

struct sign_state
{
      /** returns true if we have a signature for this key or can 
       * produce a signature for this key, else returns false. 
       */
      bool signed_by(const public_key_type &k)
      {
            //nico wdump((" publc:public_key_type")(string(k)));
            //for(auto tempkey : provided_signatures)
            //  wdump((string(tempkey.first))); //nico 签名打印
            auto itr = provided_signatures.find(k);
            if (itr == provided_signatures.end())
            {
                  auto pk = available_keys.find(k);
                  if (pk != available_keys.end())
                        return provided_signatures[k] = true;
                  return false;
            }
            return itr->second = true;
      }

      optional<map<address, public_key_type>> available_address_sigs;
      optional<map<address, public_key_type>> provided_address_sigs;

      bool signed_by(const address &a)
      {
            //wdump((" publc:address")(string(a)));                                                                 //  验证签名对应的public
            if (!available_address_sigs)
            {
                  available_address_sigs = std::map<address, public_key_type>();
                  provided_address_sigs = std::map<address, public_key_type>();
                  for (auto &item : available_keys)
                  {
                        (*available_address_sigs)[address(pts_address(item, false, 56))] = item;
                        (*available_address_sigs)[address(pts_address(item, true, 56))] = item;
                        (*available_address_sigs)[address(pts_address(item, false, 0))] = item;
                        (*available_address_sigs)[address(pts_address(item, true, 0))] = item;
                        (*available_address_sigs)[address(item)] = item;
                  }
                  for (auto &item : provided_signatures)
                  {
                        (*provided_address_sigs)[address(pts_address(item.first, false, 56))] = item.first;
                        (*provided_address_sigs)[address(pts_address(item.first, true, 56))] = item.first;
                        (*provided_address_sigs)[address(pts_address(item.first, false, 0))] = item.first;
                        (*provided_address_sigs)[address(pts_address(item.first, true, 0))] = item.first;
                        (*provided_address_sigs)[address(item.first)] = item.first;
                  }
            }
            auto itr = provided_address_sigs->find(a);
            if (itr == provided_address_sigs->end())
            {
                  auto aitr = available_address_sigs->find(a);
                  if (aitr != available_address_sigs->end())
                  {
                        auto pk = available_keys.find(aitr->second);
                        if (pk != available_keys.end())
                              return provided_signatures[aitr->second] = true;
                        return false;
                  }
            }
            return provided_signatures[itr->second] = true;
      }

      bool check_authority(account_id_type id)
      {
            if (approved_by.find(id) != approved_by.end())
                  return true;
            return check_authority(get_active(id));
      }

      /**
       *  Checks to see if we have signatures of the active authorites of
       *  the accounts specified in authority or the keys specified. 
       */
      bool check_authority(const authority *au, uint32_t depth = 0)
      {
            if (au == nullptr)
                  return false;
            const authority &auth = *au;
            
            uint32_t total_weight = 0;
            for (const auto &k : auth.key_auths)
                  if (signed_by(k.first))
                  {
                        total_weight += k.second;
                        if (total_weight >= auth.weight_threshold)
                              return true;
                  }

            for (const auto &k : auth.address_auths)
                  if (signed_by(k.first))
                  {
                        total_weight += k.second;
                        if (total_weight >= auth.weight_threshold)
                              return true;
                  }

            for (const auto &a : auth.account_auths)
            {
                  if (approved_by.find(a.first) == approved_by.end())
                  {
                        if (depth == max_recursion)
                              continue;
                        if (check_authority(get_active(a.first), depth + 1))
                        {
                              approved_by.insert(a.first);
                              total_weight += a.second;
                              if (total_weight >= auth.weight_threshold)
                                    return true;
                        }
                  }
                  else
                  {
                        total_weight += a.second;
                        if (total_weight >= auth.weight_threshold)
                              return true;
                  }
            }
            return total_weight >= auth.weight_threshold;
      }

      size_t remove_unused_signatures() //  移除没有用的签名
      {
            vector<public_key_type> remove_sigs;
            for (const auto &sig : provided_signatures)
                  if (!sig.second)
                        remove_sigs.push_back(sig.first);

            for (auto &sig : remove_sigs)
                  provided_signatures.erase(sig);

            return remove_sigs.size();
      }

      sign_state(const flat_set<public_key_type> &sigs,
                 const std::function<const authority *(account_id_type)> &a,
                 const flat_set<public_key_type> &keys = flat_set<public_key_type>())
          : get_active(a), available_keys(keys)
      {
            for (const auto &key : sigs)
                  provided_signatures[key] = false;
            approved_by.insert(GRAPHENE_TEMP_ACCOUNT);
      }

      const std::function<const authority *(account_id_type)> &get_active;
      const flat_set<public_key_type> &available_keys;

      flat_map<public_key_type, bool> provided_signatures;
      flat_set<account_id_type> approved_by;
      uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH;
};

void verify_authority(const vector<operation> &ops, const flat_set<public_key_type> &sigs,
                      //const authority temporary, // nico add 预计增加临时权限
                      const std::function<const authority *(account_id_type)> &get_active,
                      const std::function<const authority *(account_id_type)> &get_owner,
                      uint32_t max_recursion_depth,
                      bool allow_committe,
                      const flat_set<account_id_type> &active_aprovals,
                      const flat_set<account_id_type> &owner_approvals)
{
      try
      {
            flat_set<account_id_type> required_active;
            flat_set<account_id_type> required_owner;
            vector<authority> other;

            for (const auto &op : ops)
                  operation_get_required_authorities(op, required_active, required_owner, other); //  ：： 获取交易要求的权限内容

            sign_state s(sigs, get_active); // sigs为获取的签名中提供的权限   //auto get_active = [&]( account_id_type id ) { return &id(*this).active; };
            s.max_recursion = max_recursion_depth;
            for (auto &id : active_aprovals)
                  s.approved_by.insert(id);
            for (auto &id : owner_approvals)
                  s.approved_by.insert(id);
            if (!allow_committe)
                  GRAPHENE_ASSERT(required_active.find(GRAPHENE_COMMITTEE_ACCOUNT) == required_active.end(),
                                  invalid_committee_approval, "Committee account may only propose transactions");
            else
            {
                  try
                  {
                        if (s.check_authority(GRAPHENE_COMMITTEE_ACCOUNT)) // 在提议中，理事会权限将被表达为最高权限
                        {
                              wlog("The committee has approved it.");
                              return;
                        }
                  }
                  catch (...)
                  {
                        wlog("test committee authority error");
                  }
            }
            for (const auto &auth : other)
            {
                  GRAPHENE_ASSERT(s.check_authority(&auth), tx_missing_other_auth, "Missing Authority", ("auth", auth)("sigs", sigs));
            }

            // fetch all of the top level authorities
            for (auto id : required_active)
            {
                  bool hasAuthority = s.check_authority(id) || s.check_authority(get_owner(id)); // ：： 验证对应的权限
                  GRAPHENE_ASSERT(hasAuthority, tx_missing_active_auth, "Missing Active Authority ${id}", ("id", id)("auth", *get_active(id))("owner", *get_owner(id)));
            }

            for (auto id : required_owner)
            {
                  GRAPHENE_ASSERT(owner_approvals.find(id) != owner_approvals.end() ||
                                      s.check_authority(get_owner(id)),
                                  tx_missing_owner_auth, "Missing Owner Authority ${id}", ("id", id)("auth", *get_owner(id)));
            }

            GRAPHENE_ASSERT(s.remove_unused_signatures() <= 5 || allow_committe, tx_irrelevant_sig, "Unnecessary signature(s) detected"); // 检测无用的签名,考虑允许无效的签名存在,但最多不允许超过5条
      }
      FC_CAPTURE_AND_RETHROW((ops)(sigs))
}

flat_set<public_key_type> signed_transaction::get_signature_keys(const chain_id_type &chain_id) const
{
      try
      {
            auto d = sig_digest(chain_id);
            flat_set<public_key_type> result;
            for (const auto &sig : signatures)
            {
                  GRAPHENE_ASSERT(
                      result.insert(fc::ecc::public_key(sig, d)).second,
                      tx_duplicate_sig,
                      "Duplicate Signature detected");
            }
            return result;
      }
      FC_CAPTURE_AND_RETHROW()
}

set<public_key_type> signed_transaction::get_required_signatures(
    const chain_id_type &chain_id,
    const flat_set<public_key_type> &available_keys,
    const std::function<const authority *(account_id_type)> &get_active,
    const std::function<const authority *(account_id_type)> &get_owner,
    uint32_t max_recursion_depth) const
{
      flat_set<account_id_type> required_active;
      flat_set<account_id_type> required_owner;
      vector<authority> other;
      get_required_authorities(required_active, required_owner, other);

      sign_state s(get_signature_keys(chain_id), get_active, available_keys);
      s.max_recursion = max_recursion_depth;

      for (const auto &auth : other)
            s.check_authority(&auth);
      for (auto &owner : required_owner)
            s.check_authority(get_owner(owner));
      for (auto &active : required_active)
            s.check_authority(active);

      s.remove_unused_signatures();

      set<public_key_type> result;

      for (auto &provided_sig : s.provided_signatures)
            if (available_keys.find(provided_sig.first) != available_keys.end())
                  result.insert(provided_sig.first);

      return result;
}

set<public_key_type> signed_transaction::minimize_required_signatures(
    const chain_id_type &chain_id,
    const flat_set<public_key_type> &available_keys,
    const std::function<const authority *(account_id_type)> &get_active,
    const std::function<const authority *(account_id_type)> &get_owner,
    uint32_t max_recursion) const
{
      set<public_key_type> s = get_required_signatures(chain_id, available_keys, get_active, get_owner, max_recursion); // 
      flat_set<public_key_type> result(s.begin(), s.end());

      for (const public_key_type &k : s)
      {
            result.erase(k);
            try
            {
                  graphene::chain::verify_authority(operations, result, get_active, get_owner, max_recursion); //nico 验证用户权限
                  continue;                                                                                    // element stays erased if verify_authority is ok
            }
            catch (const tx_missing_owner_auth &e)
            {
            }
            catch (const tx_missing_active_auth &e)
            {
            }
            catch (const tx_missing_other_auth &e)
            {
            }
            result.insert(k);
      }
      return set<public_key_type>(result.begin(), result.end());
}

void signed_transaction::verify_authority(
    const chain_id_type &chain_id,
    const std::function<const authority *(account_id_type)> &get_active,
    const std::function<const authority *(account_id_type)> &get_owner, flat_set<public_key_type> &sigkeys,
    uint32_t max_recursion) const
{
      try
      {

            sigkeys = get_signature_keys(chain_id);
            graphene::chain::verify_authority(operations, sigkeys, get_active, get_owner, max_recursion); //验证链ID和对应的权限 并通过链ID获取签名的公钥
      }
      FC_CAPTURE_AND_RETHROW((*this))
}

} // namespace chain
} // namespace graphene
