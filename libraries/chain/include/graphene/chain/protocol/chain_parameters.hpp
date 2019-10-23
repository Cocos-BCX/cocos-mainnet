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
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/types.hpp>
#include <fc/smart_ref_fwd.hpp>

namespace graphene { namespace chain { struct fee_schedule; } }
/*
namespace fc {
   template<typename Stream, typename T> inline void pack( Stream& s, const graphene::chain::fee_schedule& value );
   template<typename Stream, typename T> inline void unpack( Stream& s, graphene::chain::fee_schedule& value );
} // namespace fc
*/

namespace graphene { namespace chain {

   typedef static_variant<>  parameter_extension; 
   struct chain_parameters
   {
      /** using a smart ref breaks the circular dependency created between operations and the fee schedule */
      smart_ref<fee_schedule> current_fees;                       ///< current schedule of fees
      uint8_t                 block_interval                      = GRAPHENE_DEFAULT_BLOCK_INTERVAL; ///< interval in seconds between blocks
      uint32_t                maintenance_interval                = GRAPHENE_DEFAULT_MAINTENANCE_INTERVAL; ///< interval in sections between blockchain maintenance events
      uint8_t                 maintenance_skip_slots              = GRAPHENE_DEFAULT_MAINTENANCE_SKIP_SLOTS; ///< number of block_intervals to skip at maintenance time
      uint32_t                committee_proposal_review_period    = GRAPHENE_DEFAULT_COMMITTEE_PROPOSAL_REVIEW_PERIOD_SEC; ///< minimum time in seconds that a proposed transaction requiring committee authority may not be signed, prior to expiration
     // uint32_t                maximum_transaction_size            = GRAPHENE_DEFAULT_MAX_TRANSACTION_SIZE; ///< maximum allowable size in bytes for a transaction
      uint32_t                maximum_block_size                  = GRAPHENE_DEFAULT_MAX_BLOCK_SIZE; ///< maximum allowable size in bytes for a block
      uint32_t                maximum_time_until_expiration       = GRAPHENE_DEFAULT_MAX_TIME_UNTIL_EXPIRATION; ///< maximum lifetime in seconds for transactions to be valid, before expiring
      uint32_t                maximum_proposal_lifetime           = GRAPHENE_DEFAULT_MAX_PROPOSAL_LIFETIME_SEC; ///< maximum lifetime in seconds for proposed transactions to be kept, before expiring
      //uint8_t                 maximum_asset_whitelist_authorities = GRAPHENE_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES; ///< maximum number of accounts which an asset may list as authorities for its whitelist OR blacklist
      uint8_t                 maximum_asset_feed_publishers       = GRAPHENE_DEFAULT_MAX_ASSET_FEED_PUBLISHERS; ///< the maximum number of feed publishers for a given asset
      uint16_t                witness_number_of_election          = GRAPHENE_DEFAULT_WITNESSE_NUMBER; ///< maximum number of active witnesses
      uint16_t                committee_number_of_election             = GRAPHENE_DEFAULT_COMMITTEE_NUMBER; ///< maximum number of active committee_members
      uint16_t                maximum_authority_membership        = GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP; ///< largest number of keys/accounts an authority can have
      uint32_t                cashback_gas_period_seconds         = GRAPHENE_DEFAULT_CASHBACK_VESTING_PERIOD_SEC; ///< time after cashback rewards are accrued before they become liquid
      uint32_t                cashback_vb_period_seconds          = GRAPHENE_DEFAULT_CASHBACK_VESTING_PERIOD_SEC;
      uint32_t                cashback_vote_period_seconds        = GRAPHENE_DEFAULT_CASHBACK_VESTING_PERIOD_SEC;
      //bool                    allow_non_member_whitelists         = false; ///< true if non-member accounts may set whitelists and blacklists; false otherwise
      share_type              witness_pay_per_block               = GRAPHENE_DEFAULT_WITNESS_PAY_PER_BLOCK; ///< CORE to be allocated to witnesses (per block)
      uint32_t                witness_pay_vesting_seconds         = GRAPHENE_DEFAULT_WITNESS_PAY_VESTING_SECONDS; ///< vesting_seconds parameter for witness VBO's
      share_type              worker_budget_per_day               = GRAPHENE_DEFAULT_WORKER_BUDGET_PER_DAY; ///< CORE to be allocated to workers (per day)
      uint16_t                accounts_per_fee_scale              = GRAPHENE_DEFAULT_ACCOUNTS_PER_FEE_SCALE; ///< number of accounts between fee scalings
      uint8_t                 account_fee_scale_bitshifts         = GRAPHENE_DEFAULT_ACCOUNT_FEE_SCALE_BITSHIFTS; ///< number of times to left bitshift account registration fee at each scaling
      uint8_t                 max_authority_depth                 = GRAPHENE_MAX_SIG_CHECK_DEPTH;
      uint16_t                maximum_run_time_ratio              = MAXIMUN_RUN_TIME_RATIO;
      uint32_t                maximum_nh_asset_order_expiration   = MAXIMUN_NH_ASSET_ORDER_EXPIRATION;
      uint32_t                assigned_task_life_cycle            = ASSIGNED_TASK_LIFE_CYCLE;
      uint32_t                crontab_suspend_threshold           = CRONTAB_SUSPEND_THRESHOLD;
      uint32_t                crontab_suspend_expiration          = CRONTAB_SUSPEND_EXPIRATION;
      share_type              witness_candidate_freeze            = WITNESS_CANDIDATE_FREEZE;
      share_type              committee_candidate_freeze          = COMMITTEE_CANDIDATE_FREEZE;
      share_type              candidate_award_budget              = CANDIDATE_AWARD_BUDGET;
      uint16_t                committee_percent_of_candidate_award= COMMITTEE_PERCENT_OF_CANDIDATE_AWARD;
      uint16_t                unsuccessful_candidates_percent     = UNSUCCESSFUL_CANDIDATES_PERCENT;
      extensions_type         extensions;

      /** defined in fee_schedule.cpp */
      void validate()const;
   };

} }  // graphene::chain

FC_REFLECT( graphene::chain::chain_parameters,
            (current_fees)
            (block_interval)
            (maintenance_interval)
            (maintenance_skip_slots)
            (committee_proposal_review_period)
            //(maximum_transaction_size)
            (maximum_block_size)
            (maximum_time_until_expiration)
            (maximum_proposal_lifetime)
            //(maximum_asset_whitelist_authorities)
            (maximum_asset_feed_publishers)
            (witness_number_of_election)
            (committee_number_of_election)
            (maximum_authority_membership)
            //(committee_percent_of_network)
            (cashback_gas_period_seconds)
            (cashback_vb_period_seconds)
            (cashback_vote_period_seconds)
            //(allow_non_member_whitelists)
            (witness_pay_per_block)
            (witness_pay_vesting_seconds)
            (worker_budget_per_day)
            (accounts_per_fee_scale)
            (account_fee_scale_bitshifts)
            (max_authority_depth)
            (maximum_run_time_ratio)
            (maximum_nh_asset_order_expiration)
            (assigned_task_life_cycle)
            (crontab_suspend_threshold)
            (crontab_suspend_expiration)
            (witness_candidate_freeze)
            (committee_candidate_freeze)
            (candidate_award_budget)
            (committee_percent_of_candidate_award)
            (unsuccessful_candidates_percent)
            (extensions)
          )
