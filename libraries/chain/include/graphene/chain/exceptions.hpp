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

#include <fc/exception/exception.hpp>
#include <graphene/chain/protocol/protocol.hpp>

#define GRAPHENE_ASSERT( expr, exc_type, FORMAT, ... )                \
   FC_MULTILINE_MACRO_BEGIN                                           \
   if( !(expr) )                                                      \
      FC_THROW_EXCEPTION( exc_type, FORMAT, __VA_ARGS__ );            \
   FC_MULTILINE_MACRO_END


#define GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( op_name )                \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _validate_exception,                                 \
      graphene::chain::operation_validate_exception,                  \
      3040000 + 100 * operation::tag< op_name ## _operation >::value, \
      #op_name "_operation validation exception"                      \
      )                                                               \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _evaluate_exception,                                 \
      graphene::chain::operation_evaluate_exception,                  \
      3050000 + 100 * operation::tag< op_name ## _operation >::value, \
      #op_name "_operation evaluation exception"                      \
      )

#define GRAPHENE_DECLARE_OP_VALIDATE_EXCEPTION( exc_name, op_name, seqnum, msg ) \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _ ## exc_name,                                       \
      graphene::chain::op_name ## _validate_exception,                \
      3040000 + 100 * operation::tag< op_name ## _operation >::value  \
         + seqnum,                                                    \
      msg                                                             \
      )

#define GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( exc_name, op_name, seqnum, msg ) \
   FC_DECLARE_DERIVED_EXCEPTION(                                      \
      op_name ## _ ## exc_name,                                       \
      graphene::chain::op_name ## _evaluate_exception,                \
      3050000 + 100 * operation::tag< op_name ## _operation >::value  \
         + seqnum,                                                    \
      msg                                                             \
      )

namespace graphene { namespace chain {

   FC_DECLARE_EXCEPTION( chain_exception, 3000000, "blockchain exception" )
   FC_DECLARE_DERIVED_EXCEPTION( database_query_exception,          graphene::chain::chain_exception, 3010000, "database query exception" )
   FC_DECLARE_DERIVED_EXCEPTION( block_validate_exception,          graphene::chain::chain_exception, 3020000, "block validation exception" )
   FC_DECLARE_DERIVED_EXCEPTION( transaction_exception,             graphene::chain::chain_exception, 3030000, "transaction validation exception" )
   FC_DECLARE_DERIVED_EXCEPTION( operation_validate_exception,      graphene::chain::chain_exception, 3040000, "operation validation exception" )
   FC_DECLARE_DERIVED_EXCEPTION( operation_evaluate_exception,      graphene::chain::chain_exception, 3050000, "operation evaluation exception" )
   FC_DECLARE_DERIVED_EXCEPTION( utility_exception,                 graphene::chain::chain_exception, 3060000, "utility method exception" )
   FC_DECLARE_DERIVED_EXCEPTION( undo_database_exception,           graphene::chain::chain_exception, 3070000, "undo database exception" )
   FC_DECLARE_DERIVED_EXCEPTION( unlinkable_block_exception,        graphene::chain::chain_exception, 3080000, "unlinkable block" )
   FC_DECLARE_DERIVED_EXCEPTION( black_swan_exception,              graphene::chain::chain_exception, 3090000, "black swan" )

   FC_DECLARE_DERIVED_EXCEPTION( tx_missing_active_auth,            graphene::chain::transaction_exception, 3030001, "missing required active authority" )
   FC_DECLARE_DERIVED_EXCEPTION( tx_missing_owner_auth,             graphene::chain::transaction_exception, 3030002, "missing required owner authority" )
   FC_DECLARE_DERIVED_EXCEPTION( tx_missing_other_auth,             graphene::chain::transaction_exception, 3030003, "missing required other authority" )
   FC_DECLARE_DERIVED_EXCEPTION( tx_irrelevant_sig,                 graphene::chain::transaction_exception, 3030004, "irrelevant signature included" )
   FC_DECLARE_DERIVED_EXCEPTION( tx_duplicate_sig,                  graphene::chain::transaction_exception, 3030005, "duplicate signature included" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_committee_approval,        graphene::chain::transaction_exception, 3030006, "committee account cannot directly approve transaction" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_fee,                  graphene::chain::transaction_exception, 3030007, "insufficient fee" )

   FC_DECLARE_DERIVED_EXCEPTION( invalid_pts_address,               graphene::chain::utility_exception, 3060001, "invalid pts address" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_feeds,                graphene::chain::chain_exception, 37006, "insufficient feeds" )

   FC_DECLARE_DERIVED_EXCEPTION( pop_empty_chain,                   graphene::chain::undo_database_exception, 3070001, "there are no blocks to pop" )

   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( transfer );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( from_account_not_whitelisted, transfer, 1, "owner mismatch" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( to_account_not_whitelisted, transfer, 2, "owner mismatch" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( restricted_transfer_asset, transfer, 3, "restricted transfer asset" )

   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( limit_order_create );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( limit_order_cancel );
   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( call_order_update );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( unfilled_margin_call, call_order_update, 1, "Updating call order would trigger a margin call that cannot be fully filled" )

   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_create );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( max_auth_exceeded, account_create, 1, "Exceeds max authority fan-out" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( auth_account_not_found, account_create, 2, "Auth account not found" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( buyback_incorrect_issuer, account_create, 3, "Incorrect issuer specified for account" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( buyback_already_exists, account_create, 4, "Cannot create buyback for asset which already has buyback" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( buyback_too_many_markets, account_create, 5, "Too many buyback markets" )

   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_update );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( max_auth_exceeded, account_update, 1, "Exceeds max authority fan-out" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( auth_account_not_found, account_update, 2, "Auth account not found" )

   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_whitelist );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_upgrade );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( account_transfer );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_create );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_update );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_update_bitasset );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_update_feed_producers );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_issue );

   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_reserve );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( invalid_on_mia, asset_reserve, 1, "invalid on mia" )

   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_fund_fee_pool );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_settle );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_global_settle );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( asset_publish_feed );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( committee_member_create );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( witness_create );

   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( proposal_create );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( review_period_required, proposal_create, 1, "review_period required" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( review_period_insufficient, proposal_create, 2, "review_period insufficient" )

   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( proposal_update );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( proposal_delete );
 
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( fill_order );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( global_parameters_update );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( vesting_balance_create );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( vesting_balance_withdraw );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( worker_create );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( custom );
   //GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( assert );

   GRAPHENE_DECLARE_OP_BASE_EXCEPTIONS( balance_claim );
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( claimed_too_often, balance_claim, 1, "balance claimed too often" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( invalid_claim_amount, balance_claim, 2, "invalid claim amount" )
   GRAPHENE_DECLARE_OP_EVALUATE_EXCEPTION( owner_mismatch, balance_claim, 3, "owner mismatch" )

   /*
   FC_DECLARE_DERIVED_EXCEPTION( addition_overflow,                 graphene::chain::chain_exception, 30002, "addition overflow" )
   FC_DECLARE_DERIVED_EXCEPTION( subtraction_overflow,              graphene::chain::chain_exception, 30003, "subtraction overflow" )
   FC_DECLARE_DERIVED_EXCEPTION( asset_type_mismatch,               graphene::chain::chain_exception, 30004, "asset/price mismatch" )
   FC_DECLARE_DERIVED_EXCEPTION( unsupported_chain_operation,       graphene::chain::chain_exception, 30005, "unsupported chain operation" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_transaction,               graphene::chain::chain_exception, 30006, "unknown transaction" )
   FC_DECLARE_DERIVED_EXCEPTION( duplicate_transaction,             graphene::chain::chain_exception, 30007, "duplicate transaction" )
   FC_DECLARE_DERIVED_EXCEPTION( zero_amount,                       graphene::chain::chain_exception, 30008, "zero amount" )
   FC_DECLARE_DERIVED_EXCEPTION( zero_price,                        graphene::chain::chain_exception, 30009, "zero price" )
   FC_DECLARE_DERIVED_EXCEPTION( asset_divide_by_self,              graphene::chain::chain_exception, 30010, "asset divide by self" )
   FC_DECLARE_DERIVED_EXCEPTION( asset_divide_by_zero,              graphene::chain::chain_exception, 30011, "asset divide by zero" )
   FC_DECLARE_DERIVED_EXCEPTION( new_database_version,              graphene::chain::chain_exception, 30012, "new database version" )
   FC_DECLARE_DERIVED_EXCEPTION( unlinkable_block,                  graphene::chain::chain_exception, 30013, "unlinkable block" )
   FC_DECLARE_DERIVED_EXCEPTION( price_out_of_range,                graphene::chain::chain_exception, 30014, "price out of range" )

   FC_DECLARE_DERIVED_EXCEPTION( block_numbers_not_sequential,      graphene::chain::chain_exception, 30015, "block numbers not sequential" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_previous_block_id,         graphene::chain::chain_exception, 30016, "invalid previous block" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_block_time,                graphene::chain::chain_exception, 30017, "invalid block time" )
   FC_DECLARE_DERIVED_EXCEPTION( time_in_past,                      graphene::chain::chain_exception, 30018, "time is in the past" )
   FC_DECLARE_DERIVED_EXCEPTION( time_in_future,                    graphene::chain::chain_exception, 30019, "time is in the future" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_block_digest,              graphene::chain::chain_exception, 30020, "invalid block digest" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_committee_member_signee,           graphene::chain::chain_exception, 30021, "invalid committee_member signee" )
   FC_DECLARE_DERIVED_EXCEPTION( failed_checkpoint_verification,    graphene::chain::chain_exception, 30022, "failed checkpoint verification" )
   FC_DECLARE_DERIVED_EXCEPTION( wrong_chain_id,                    graphene::chain::chain_exception, 30023, "wrong chain id" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_block,                     graphene::chain::chain_exception, 30024, "unknown block" )
   FC_DECLARE_DERIVED_EXCEPTION( block_older_than_undo_history,     graphene::chain::chain_exception, 30025, "block is older than our undo history allows us to process" )

   FC_DECLARE_EXCEPTION( evaluation_error, 31000, "Evaluation Error" )
   FC_DECLARE_DERIVED_EXCEPTION( negative_deposit,                  graphene::chain::evaluation_error, 31001, "negative deposit" )
   FC_DECLARE_DERIVED_EXCEPTION( not_a_committee_member,                    graphene::chain::evaluation_error, 31002, "not a committee_member" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_balance_record,            graphene::chain::evaluation_error, 31003, "unknown balance record" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_funds,                graphene::chain::evaluation_error, 31004, "insufficient funds" )
   FC_DECLARE_DERIVED_EXCEPTION( missing_signature,                 graphene::chain::evaluation_error, 31005, "missing signature" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_claim_password,            graphene::chain::evaluation_error, 31006, "invalid claim password" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_withdraw_condition,        graphene::chain::evaluation_error, 31007, "invalid withdraw condition" )
   FC_DECLARE_DERIVED_EXCEPTION( negative_withdraw,                 graphene::chain::evaluation_error, 31008, "negative withdraw" )
   FC_DECLARE_DERIVED_EXCEPTION( not_an_active_committee_member,            graphene::chain::evaluation_error, 31009, "not an active committee_member" )
   FC_DECLARE_DERIVED_EXCEPTION( expired_transaction,               graphene::chain::evaluation_error, 31010, "expired transaction" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_transaction_expiration,    graphene::chain::evaluation_error, 31011, "invalid transaction expiration" )
   FC_DECLARE_DERIVED_EXCEPTION( oversized_transaction,             graphene::chain::evaluation_error, 31012, "transaction exceeded the maximum transaction size" )

   FC_DECLARE_DERIVED_EXCEPTION( invalid_account_name,              graphene::chain::evaluation_error, 32001, "invalid account name" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_account_id,                graphene::chain::evaluation_error, 32002, "unknown account id" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_account_name,              graphene::chain::evaluation_error, 32003, "unknown account name" )
   FC_DECLARE_DERIVED_EXCEPTION( missing_parent_account_signature,  graphene::chain::evaluation_error, 32004, "missing parent account signature" )
   FC_DECLARE_DERIVED_EXCEPTION( parent_account_retracted,          graphene::chain::evaluation_error, 32005, "parent account retracted" )
   FC_DECLARE_DERIVED_EXCEPTION( account_expired,                   graphene::chain::evaluation_error, 32006, "account expired" )
   FC_DECLARE_DERIVED_EXCEPTION( account_already_registered,        graphene::chain::evaluation_error, 32007, "account already registered" )
   FC_DECLARE_DERIVED_EXCEPTION( account_key_in_use,                graphene::chain::evaluation_error, 32008, "account key already in use" )
   FC_DECLARE_DERIVED_EXCEPTION( account_retracted,                 graphene::chain::evaluation_error, 32009, "account retracted" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_parent_account_name,       graphene::chain::evaluation_error, 32010, "unknown parent account name" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_committee_member_slate,            graphene::chain::evaluation_error, 32011, "unknown committee_member slate" )
   FC_DECLARE_DERIVED_EXCEPTION( too_may_committee_members_in_slate,        graphene::chain::evaluation_error, 32012, "too many committee_members in slate" )
   FC_DECLARE_DERIVED_EXCEPTION( pay_balance_remaining,             graphene::chain::evaluation_error, 32013, "pay balance remaining" )

   FC_DECLARE_DERIVED_EXCEPTION( not_a_committee_member_signature,          graphene::chain::evaluation_error, 33002, "not committee_members signature" )

   FC_DECLARE_DERIVED_EXCEPTION( invalid_precision,                 graphene::chain::evaluation_error, 35001, "invalid precision" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_asset_symbol,              graphene::chain::evaluation_error, 35002, "invalid asset symbol" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_asset_id,                  graphene::chain::evaluation_error, 35003, "unknown asset id" )
   FC_DECLARE_DERIVED_EXCEPTION( asset_symbol_in_use,               graphene::chain::evaluation_error, 35004, "asset symbol in use" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_asset_amount,              graphene::chain::evaluation_error, 35005, "invalid asset amount" )
   FC_DECLARE_DERIVED_EXCEPTION( negative_issue,                    graphene::chain::evaluation_error, 35006, "negative issue" )
   FC_DECLARE_DERIVED_EXCEPTION( over_issue,                        graphene::chain::evaluation_error, 35007, "over issue" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_asset_symbol,              graphene::chain::evaluation_error, 35008, "unknown asset symbol" )
   FC_DECLARE_DERIVED_EXCEPTION( asset_id_in_use,                   graphene::chain::evaluation_error, 35009, "asset id in use" )
   FC_DECLARE_DERIVED_EXCEPTION( not_user_issued,                   graphene::chain::evaluation_error, 35010, "not user issued" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_asset_name,                graphene::chain::evaluation_error, 35011, "invalid asset name" )

   FC_DECLARE_DERIVED_EXCEPTION( committee_member_vote_limit,               graphene::chain::evaluation_error, 36001, "committee_member_vote_limit" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_fee,                  graphene::chain::evaluation_error, 36002, "insufficient fee" )
   FC_DECLARE_DERIVED_EXCEPTION( negative_fee,                      graphene::chain::evaluation_error, 36003, "negative fee" )
   FC_DECLARE_DERIVED_EXCEPTION( missing_deposit,                   graphene::chain::evaluation_error, 36004, "missing deposit" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_relay_fee,            graphene::chain::evaluation_error, 36005, "insufficient relay fee" )

   FC_DECLARE_DERIVED_EXCEPTION( invalid_market,                    graphene::chain::evaluation_error, 37001, "invalid market" )
   FC_DECLARE_DERIVED_EXCEPTION( unknown_market_order,              graphene::chain::evaluation_error, 37002, "unknown market order" )
   FC_DECLARE_DERIVED_EXCEPTION( shorting_base_shares,              graphene::chain::evaluation_error, 37003, "shorting base shares" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_collateral,           graphene::chain::evaluation_error, 37004, "insufficient collateral" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_depth,                graphene::chain::evaluation_error, 37005, "insufficient depth" )
   FC_DECLARE_DERIVED_EXCEPTION( insufficient_feeds,                graphene::chain::evaluation_error, 37006, "insufficient feeds" )
   FC_DECLARE_DERIVED_EXCEPTION( invalid_feed_price,                graphene::chain::evaluation_error, 37007, "invalid feed price" )

   FC_DECLARE_DERIVED_EXCEPTION( price_multiplication_overflow,     graphene::chain::evaluation_error, 38001, "price multiplication overflow" )
   FC_DECLARE_DERIVED_EXCEPTION( price_multiplication_underflow,    graphene::chain::evaluation_error, 38002, "price multiplication underflow" )
   FC_DECLARE_DERIVED_EXCEPTION( price_multiplication_undefined,    graphene::chain::evaluation_error, 38003, "price multiplication undefined product 0*inf" )
   */

   #define GRAPHENE_RECODE_EXC( cause_type, effect_type ) \
      catch( const cause_type& e ) \
      { throw( effect_type( e.what(), e.get_log() ) ); }

} } // graphene::chain
