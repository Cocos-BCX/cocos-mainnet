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

#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/asset.hpp>
#include <graphene/chain/protocol/authority.hpp>
#include <graphene/chain/protocol/vote.hpp>
#include <graphene/chain/protocol/memo.hpp>

namespace graphene
{
namespace chain
{

/**
    *  @defgroup operations Operations
    *  @ingroup transactions Transactions
    *  @brief A set of valid comands for mutating the globally shared state.
    *
    *  An operation can be thought of like a function that will modify the global
    *  shared state of the blockchain.  The members of each struct are like function
    *  arguments and each operation can potentially generate a return value.
    *
    *  Operations can be grouped into transactions (@ref transaction) to ensure that they occur
    *  in a particular order and that all operations apply successfully or
    *  no operations apply.
    *
    *  Each operation is a fully defined state transition and can exist in a transaction on its own.
    *
    *  @section operation_design_principles Design Principles
    *
    *  Operations have been carefully designed to include all of the information necessary to
    *  interpret them outside the context of the blockchain.   This means that information about
    *  current chain state is included in the operation even though it could be inferred from
    *  a subset of the data.   This makes the expected outcome of each operation well defined and
    *  easily understood without access to chain state.
    *
    *  @subsection balance_calculation Balance Calculation Principle
    *
    *    We have stipulated that the current account balance may be entirely calculated from
    *    just the subset of operations that are relevant to that account.  There should be
    *    no need to process the entire blockchain inorder to know your account's balance.
    *
    *  @subsection fee_calculation Explicit Fee Principle
    *
    *    Blockchain fees can change from time to time and it is important that a signed
    *    transaction explicitly agree to the fees it will be paying.  This aids with account
    *    balance updates and ensures that the sender agreed to the fee prior to making the
    *    transaction.
    *
    *  @subsection defined_authority Explicit Authority
    *
    *    Each operation shall contain enough information to know which accounts must authorize
    *    the operation.  This principle enables authority verification to occur in a centralized,
    *    optimized, and parallel manner.
    *
    *  @subsection relevancy_principle Explicit Relevant Accounts
    *
    *    Each operation contains enough information to enumerate all accounts for which the
    *    operation should apear in its account history.  This principle enables us to easily
    *    define and enforce the @balance_calculation. This is superset of the @ref defined_authority
    *
    *  @{
    */
struct base_result
{
  base_result(){};
  optional<vector<asset>> fees;
};

struct void_result:public base_result
{
  void_result(){}
  uint64_t real_running_time = 0;
};
typedef struct token_affected
{
  account_id_type affected_account;
  asset affected_asset;
} token_affected;

typedef struct nht_affected
{
  account_id_type affected_account;
  nh_asset_id_type affected_item;
  uint64_t action;
  optional<std::pair<string, string>> modified;
} nht_affected;
typedef struct contract_memo_message
{
  account_id_type affected_account;
  graphene::chain::memo_data memo;
} contract_memo_message;

typedef struct contract_logger
{
  account_id_type affected_account;
  string message;
  contract_logger() {}
  contract_logger(account_id_type aft) : affected_account(aft) {}
} contract_logger;
typedef struct logger_result:public base_result
{
  string message;
  uint64_t real_running_time = 0;
  logger_result(string message) : message(message) {}
  logger_result() {}
} logger_result;

typedef struct error_result:public base_result
{
  int64_t error_code = 0;
  string message;
  uint64_t real_running_time = 0;
  error_result(int64_t error_code, string message) : error_code(error_code), message(message) {}
  error_result() {}
} error_result;

struct contract_result;

typedef fc::static_variant<token_affected, nht_affected, contract_memo_message, contract_logger, contract_result> contract_affected_type;

typedef struct contract_result:public base_result
{
public:
  contract_id_type contract_id;
  vector<contract_affected_type> contract_affecteds;
  optional<uint64_t> real_running_time = {};
  bool existed_pv = false;
  vector<char> process_value;
  uint64_t relevant_datasize=0;
} contract_result;

struct contract_affected_type_visitor
{
  typedef vector<account_id_type> result_type;
  template <typename contract_affected_type_op>
  result_type operator()(contract_affected_type_op &op)
  {
    return vector<account_id_type>{op.affected_account};
  }
  result_type operator()(contract_result &op)
  {
    vector<account_id_type> result;
    contract_affected_type_visitor visitor;
    for (auto var : op.contract_affecteds)
    {
      auto temp = var.visit(visitor);
      result.insert(result.end(), temp.begin(), temp.end());
    }
    return result;
  }
};

struct object_id_result:public base_result
{
  object_id_type result;
  uint64_t real_running_time = 0;
  object_id_result(object_id_type id) : result(id) {}
  object_id_result() {}
};

struct asset_result:public base_result
{
  asset result;
  uint64_t real_running_time = 0;
  asset_result() {}
  asset_result(asset result) : result(result) {}
};

typedef fc::static_variant<error_result, void_result, object_id_result, asset_result, contract_result, logger_result> operation_result;

struct base_operation
{
  template <typename T>
  share_type calculate_fee(const T &params) const
  {
    return params.fee;
  }
  void get_required_authorities(vector<authority> &) const {}
  void get_required_active_authorities(flat_set<account_id_type> &) const {} // 事物所需要的权限
  void get_required_owner_authorities(flat_set<account_id_type> &) const {}
  void validate() const {}

  static uint64_t calculate_data_fee(uint64_t bytes, uint64_t price_per_kbyte);
};

struct operation_result_visitor_set_runtime
{
  typedef void result_type;
  operation_result_visitor_set_runtime(uint64_t real_running_time) : run_time(real_running_time){};
  template <typename operation_result_object>
  void operator()(operation_result_object &op)
  {
    op.real_running_time = run_time;
  }
  void operator()(contract_result &op)
  {
    op.real_running_time = run_time;
  }
  void operator()(error_result &op)
  {
    op.real_running_time = run_time;
  }
  int64_t run_time = 0; //运行时长
};

struct operation_result_visitor_get_runtime
{
  typedef uint64_t result_type;
  template <typename operation_result_object>
  result_type operator()(operation_result_object &op)
  {
    return op.real_running_time;
  }
  result_type operator()(contract_result &op)
  {
    if (op.real_running_time)
      return *op.real_running_time;
    else
      return 0;
  }
};
/**
    *  For future expansion many structus include a single member of type
    *  extensions_type that can be changed when updating a protocol.  You can
    *  always add new types to a static_variant without breaking backward
    *  compatibility.   
    */
typedef fc::static_variant<void_t> future_extensions;

/**
    *  A flat_set is used to make sure that only one extension of
    *  each type is added and that they are added in order.  
    *  
    *  @note static_variant compares only the type tag and not the 
    *  content.
    */
// typedef flat_set<future_extensions> extensions_type;
typedef vector<std::string> extensions_type;
//typedef flat_map<asset_id_type,std::string> extensions_type;
//typedef vector<optional<GameData>> extensions_type;
///@}

} // namespace chain
} // namespace graphene
FC_REFLECT_TYPENAME(graphene::chain::operation_result)
FC_REFLECT_TYPENAME(graphene::chain::contract_affected_type)
FC_REFLECT_TYPENAME(graphene::chain::future_extensions)
FC_REFLECT(graphene::chain::base_result, (fees))
FC_REFLECT_DERIVED(graphene::chain::void_result,(graphene::chain::base_result),(real_running_time))
FC_REFLECT(graphene::chain::token_affected, (affected_account)(affected_asset))
FC_REFLECT_DERIVED(graphene::chain::object_id_result,(graphene::chain::base_result), (result)(real_running_time))
FC_REFLECT_DERIVED(graphene::chain::asset_result,(graphene::chain::base_result), (result)(real_running_time))
FC_REFLECT(graphene::chain::nht_affected, (affected_account)(affected_item)(action)(modified))
FC_REFLECT(graphene::chain::contract_memo_message, (affected_account)(memo))
FC_REFLECT(graphene::chain::contract_logger, (affected_account)(message))
FC_REFLECT_DERIVED(graphene::chain::contract_result,(graphene::chain::base_result), (contract_id)(contract_affecteds)(real_running_time)(existed_pv)(process_value)(relevant_datasize))
FC_REFLECT_DERIVED(graphene::chain::logger_result,(graphene::chain::base_result), (message)(real_running_time))
FC_REFLECT_DERIVED(graphene::chain::error_result,(graphene::chain::base_result), (error_code)(message)(real_running_time))
