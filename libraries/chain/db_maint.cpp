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

#include <boost/multiprecision/integer.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <graphene/chain/database.hpp>

#include <graphene/chain/hardfork.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/budget_record_object.hpp>
#include <graphene/chain/chain_property_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/global_property_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/special_authority_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/vote_count.hpp>
#include <graphene/chain/witness_object.hpp>
#include <graphene/chain/worker_object.hpp>

namespace graphene
{
namespace chain
{

template <typename ObjectType>
vector<std::reference_wrapper<const ObjectType>> database::sort_votable_objects(vector<std::reference_wrapper<const ObjectType>> &refs, size_t count) const
{
      /*
      using ObjectType = typename Index::object_type;
      const auto &all_objects_indices = get_index_type<Index>().indices();
      const auto &all_objects=all_objects_indices.get<by_work_status>().equal_range(true);
      count = std::min(count, boost::distance(all_objects));
      
      refs.reserve(all_objects.size());
      //nico log::遍历witness、commit等对象并根据票据排序，可在此插入筛选条件
      std::transform(all_objects.first, all_objects.secoend,
                     std::back_inserter(refs),
                     [](const ObjectType &o) { return std::cref(o); });
      */
      count = std::min(count, refs.size());
      std::partial_sort(refs.begin(), refs.begin() + count, refs.end(),
                        [this](const ObjectType &a, const ObjectType &b) -> bool {
                              if (a.total_votes != b.total_votes)
                                    return a.total_votes > b.total_votes;
                              return a.vote_id< b.vote_id;
                        });

      //refs.resize(count, refs.front());
      vector<std::reference_wrapper<const ObjectType>> ret;
      ret.insert(ret.begin(), refs.begin(), refs.begin() + count);
      return ret;
}

template <class... Types>
void database::perform_account_maintenance(std::tuple<Types...> helpers)
{
      const auto &idx = get_index_type<account_index>().indices().get<by_name>();
      for (const account_object &a : idx)
            if(a.options.votes.size())
                  detail::for_each(helpers, a, detail::gen_seq<sizeof...(Types)>()); //统计账户的投票
}

/// @brief A visitor for @ref worker_type which calls pay_worker on the worker within
struct worker_pay_visitor
{
private:
      share_type pay;
      database &db;

public:
      worker_pay_visitor(share_type pay, database &db)
          : pay(pay), db(db) {}

      typedef void result_type;
      template <typename W>
      void operator()(W &worker) const
      {
            worker.pay_worker(pay, db);
      }
};


void database::pay_workers(share_type &budget)
{
      auto index= get_index_type<worker_index>().indices().get<by_completed>().equal_range(false);
      vector<std::reference_wrapper<const worker_object>> active_worker_refs;
      auto current_settlement_time = head_block_time();
      for(auto& worker_ob:boost::make_iterator_range(index.first,index.second))
      {
            if (worker_ob.work_end_date<=current_settlement_time)
            {
                  modify(worker_ob,[](worker_object &o){o.completed=true;}); 
                  
                  current_settlement_time=worker_ob.work_end_date;
            }else if(worker_ob.is_active(current_settlement_time))
            {
                  active_worker_refs.emplace_back(std::cref(worker_ob));      
            }
                  
                 
      }
      for (uint32_t i = 0; i < active_worker_refs.size(); ++i)
      {
            const worker_object &active_worker = active_worker_refs[i];
            if(!active_worker.issuance_or_destroy()&& budget <=share_type(0))
                  continue;
            share_type requested_pay = active_worker.daily_pay;
            if (current_settlement_time - get_dynamic_global_properties().last_budget_time != fc::days(1))
            {
                  fc::uint128 pay(requested_pay.value);
                  pay *= (current_settlement_time - get_dynamic_global_properties().last_budget_time).count();
                  pay /= fc::days(1).count();
                  requested_pay = pay.to_uint64();
            }

            share_type actual_pay = std::min(budget, requested_pay);
            modify(active_worker, [&](worker_object &w) {
                  w.worker.visit(worker_pay_visitor(actual_pay, *this));
            });
            if(!active_worker.issuance_or_destroy())
                  budget -= actual_pay;
      }
}

void database::pay_candidates(share_type &budget, const uint16_t &committee_percent_of_candidate_award, const uint16_t &unsuccessful_candidates_percent)
{
      fc::uint128 temp_value = budget.value * committee_percent_of_candidate_award;
      share_type committee_ratio = (temp_value / GRAPHENE_100_PERCENT).to_uint64();
      share_type committee_cumulative = 0, witness_cumulative = 0, unsuccessful_candidates_cumulative = 0;
      temp_value = (budget - committee_ratio).value * unsuccessful_candidates_percent;
      share_type unsuccessful_candidates_ratio = (temp_value / GRAPHENE_100_PERCENT).to_uint64();
      share_type witness_ratio = budget - committee_ratio - unsuccessful_candidates_ratio;
      auto &committee_relaxed = get(GRAPHENE_RELAXED_COMMITTEE_ACCOUNT);
      for (auto &active_committee : committee_relaxed.active.account_auths)
      {
            double prop = ((double)active_committee.second) / 2 / committee_relaxed.active.weight_threshold;
            share_type proportion = prop * ((double)committee_ratio.value);
            FC_ASSERT(proportion <= committee_ratio && committee_cumulative <= committee_ratio);
            adjust_balance(active_committee.first, proportion);
            committee_cumulative += proportion;
      }
      auto &witness_account = get(GRAPHENE_WITNESS_ACCOUNT);
      for (auto &active_witness : witness_account.active.account_auths)
      {
            double prop = ((double)active_witness.second) / 2 / witness_account.active.weight_threshold;
            share_type proportion = prop * ((double)witness_ratio.value);
            FC_ASSERT(proportion <= witness_ratio && witness_cumulative <= witness_ratio);
            adjust_balance(active_witness.first, proportion);
            witness_cumulative += proportion;
      }
      auto &unsuccessful_candidates = get(unsuccessful_candidates_id_type()).unsuccessful_candidates;
      if (unsuccessful_candidates.size())
      {
            share_type proportion = ((double)unsuccessful_candidates_ratio.value) / unsuccessful_candidates.size();
            unsuccessful_candidates_cumulative = proportion * unsuccessful_candidates.size();
            FC_ASSERT(unsuccessful_candidates_cumulative<=unsuccessful_candidates_ratio);
            for (auto &unsuccessful_candidate : unsuccessful_candidates)
                  adjust_balance(unsuccessful_candidate, proportion);
      }
      budget=budget-(committee_cumulative+witness_cumulative+unsuccessful_candidates_cumulative);

}

void database::update_active_witnesses() //跟新出块人投票
{
      try
      {
            const global_property_object &gpo = get_global_properties();
            const auto &all_witnesses = get_index_type<witness_index>().indices().get<by_work_status>().equal_range(true);
            vector<std::reference_wrapper<const witness_object>> refs;
            std::transform(all_witnesses.first, all_witnesses.second,
                           std::back_inserter(refs),
                           [](const witness_object &o) { return std::cref(o); });
            auto wits = sort_votable_objects<witness_object>(refs, gpo.parameters.witness_number_of_election);
            if(wits.size()/2==0)wits.pop_back();
            flat_set<account_id_type> unsuccessful_candidates_temp;
            uint index = 0;
            for (const witness_object &wit : refs)
            {
                  if (wits.size() < ++index)
                  {
                        if (!vote_result[wit.witness_account])
                              continue;
                  }
                  else
                  {
                        if (!vote_result[wit.witness_account])
                              vote_result[wit.witness_account] = true;
                  }
            }

            //出块人数量变更

            // Update witness authority
            modify(get(GRAPHENE_WITNESS_ACCOUNT), [&](account_object &a) {
                  {
                        vote_counter vc; // 更新见证人公共账户
                        for (const witness_object &wit : wits)
                              vc.add(wit.witness_account, wit.total_votes);
                        vc.finish(a.active);
                  }
            });

            modify(gpo, [&](global_property_object &gp) {
                  gp.active_witnesses.clear();
                  gp.active_witnesses.reserve(wits.size());
                  std::transform(wits.begin(), wits.end(),
                                 std::inserter(gp.active_witnesses, gp.active_witnesses.end()),
                                 [](const witness_object &w) {
                                       return w.id;
                                 });
            });
      }
      FC_CAPTURE_AND_RETHROW()
}

void database::update_active_committee_members()
{
      try
      {
            const global_property_object &gpo = get_global_properties();
            const auto &all_committee = get_index_type<committee_member_index>().indices().get<by_work_status>().equal_range(true);
            vector<std::reference_wrapper<const committee_member_object>> refs;
            std::transform(all_committee.first, all_committee.second,
                           std::back_inserter(refs),
                           [](const committee_member_object &o) { return std::cref(o); });
            auto committee_members = sort_votable_objects<committee_member_object>(refs,(size_t)gpo.parameters.committee_number_of_election);
            flat_set<account_id_type> unsuccessful_candidates_temp;
            if(committee_members.size()/2==0)committee_members.pop_back();
            uint index = 0;
            for (const committee_member_object &del : refs)
            {
                  if (committee_members.size() < ++index)
                  {
                        if (!vote_result[del.committee_member_account])
                              continue;
                  }
                  else
                  {
                        if (!vote_result[del.committee_member_account])
                              vote_result[del.committee_member_account] = true;
                  }
            }

            // Update committee authorities
            if (!committee_members.empty())
            {
                  modify(get(GRAPHENE_COMMITTEE_ACCOUNT), [&](account_object &a) {
                        {
                              vote_counter vc; // 更新理事会公共账户权限
                              for (const committee_member_object &cm : committee_members)
                                    vc.add(cm.committee_member_account,1);
                              vc.finish(a.active);
                        }
                  });
                  modify(get(GRAPHENE_RELAXED_COMMITTEE_ACCOUNT), [&](account_object &a) {
                        {
                              vote_counter vc; // 更新理事会公共账户权限
                              for (const committee_member_object &cm : committee_members)
                                    vc.add(cm.committee_member_account,cm.total_votes);
                              vc.finish(a.active);
                        }
                  });
            }
            modify(gpo, [&](global_property_object &gp) {
                  gp.active_committee_members.clear();
                  std::transform(committee_members.begin(), committee_members.end(),
                                 std::inserter(gp.active_committee_members, gp.active_committee_members.begin()),
                                 [](const committee_member_object &d) { return d.id; });
            });
      }
      FC_CAPTURE_AND_RETHROW()
}

void database::initialize_budget_record(fc::time_point_sec now, budget_record &rec) const
{
      const dynamic_global_property_object &dpo = get_dynamic_global_properties();
      const asset_object &core = asset_id_type(0)(*this);
      const asset_dynamic_data_object &core_dd = core.dynamic_asset_data_id(*this);

      rec.from_initial_reserve = core.reserved(*this);
      rec.from_accumulated_fees = core_dd.accumulated_fees;
      rec.from_unused_witness_budget = dpo.witness_budget;

      if ((dpo.last_budget_time == fc::time_point_sec()) || (now <= dpo.last_budget_time))
      {
            rec.total_budget=rec.from_initial_reserve + core_dd.accumulated_fees;
            rec.time_since_last_budget = 0;
            return;
      }

      int64_t dt = (now - dpo.last_budget_time).to_seconds();
      rec.time_since_last_budget = uint64_t(dt);

      // We'll consider accumulated_fees to be reserved at the BEGINNING
      // of the maintenance interval.  However, for speed we only
      // call modify() on the asset_dynamic_data_object once at the
      // end of the maintenance interval.  Thus the accumulated_fees
      // are available for the budget at this point, but not included
      // in core.reserved().
      share_type reserve = rec.from_initial_reserve + core_dd.accumulated_fees;
      // Similarly, we consider leftover witness_budget to be burned
      // at the BEGINNING of the maintenance interval.
      reserve += dpo.witness_budget;

      fc::uint128_t budget_u128 = reserve.value;
      budget_u128 *= uint64_t(dt);
      budget_u128 *= GRAPHENE_CORE_ASSET_CYCLE_RATE;
      //nico log:: reserve*seconds*(GRAPHENE_CORE_ASSET_CYCLE_RATE+1)/GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS
      //round up to the nearest satoshi -- this is necessary to ensure
      //  there isn't an "untouchable" reserve, and we will eventually
      //  be able to use the entire reserve
      budget_u128 += ((uint64_t(1) << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS) - 1);
      budget_u128 >>= GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS;
      share_type budget;
      if (budget_u128 < reserve.value)
            rec.total_budget = share_type(budget_u128.to_uint64());
      else
            rec.total_budget = reserve;

      return;
}

/**
 * Update the budget for witnesses and workers.
 */
void database::process_budget(const global_property_object old_gpo)
{
      try
      {
            const global_property_object &gpo = get_global_properties();
            const dynamic_global_property_object &dpo = get_dynamic_global_properties();
            const asset_dynamic_data_object &core =
                asset_id_type(0)(*this).dynamic_asset_data_id(*this);
            fc::time_point_sec now = head_block_time();

            int64_t time_to_maint = (dpo.next_maintenance_time - now).to_seconds();
            //
            // The code that generates the next maintenance time should
            //   only produce a result in the future.  If this assert
            //   fails, then the next maintenance time algorithm is buggy.
            //
            assert(time_to_maint > 0);
            //
            // Code for setting chain parameters should validate
            //   block_interval > 0 (as well as the humans proposing /
            //   voting on changes to block interval).
            //
            assert(gpo.parameters.block_interval > 0);
            uint64_t blocks_to_maint = (uint64_t(time_to_maint) + gpo.parameters.block_interval - 1) / gpo.parameters.block_interval;

            // blocks_to_maint > 0 because time_to_maint > 0,
            // which means numerator is at least equal to block_interval

            budget_record rec;
            initialize_budget_record(now, rec);
            share_type available_funds = rec.total_budget;

            share_type witness_budget = gpo.parameters.witness_pay_per_block.value * blocks_to_maint;
            rec.requested_witness_budget = witness_budget;
            witness_budget = std::min(witness_budget, available_funds);
            rec.witness_budget = witness_budget;
            available_funds -= witness_budget;

            fc::uint128_t worker_budget_u128 = gpo.parameters.worker_budget_per_day.value;
            worker_budget_u128 *= uint64_t(time_to_maint);
            worker_budget_u128 /= 60 * 60 * 24;

            share_type worker_budget;
            if (worker_budget_u128 >= available_funds.value)
                  worker_budget = available_funds;
            else
                  worker_budget = worker_budget_u128.to_uint64();
            rec.worker_budget = worker_budget;
            available_funds -= worker_budget;
            share_type leftover_worker_funds=0;
            share_type leftover_candidates_budget=0;
            rec.candidates_budget=std::min(gpo.parameters.candidate_award_budget, available_funds);
            auto next_budget_record_id=get_index(implementation_ids,impl_budget_record_object_type).get_next_id();
            optional<budget_record> last_rec;
            if(next_budget_record_id.instance()>=1)
            {
                  last_rec=get( budget_record_id_type(next_budget_record_id.instance()-1)).record;
                  leftover_worker_funds= last_rec->worker_budget;
                  pay_workers(leftover_worker_funds); 
                  leftover_candidates_budget=last_rec->candidates_budget;
                  pay_candidates(leftover_candidates_budget,old_gpo.parameters.committee_percent_of_candidate_award,old_gpo.parameters.unsuccessful_candidates_percent);
                 
            }
            rec.leftover_candidates_budget=leftover_candidates_budget;
            rec.leftover_worker_funds = leftover_worker_funds;
            available_funds += leftover_worker_funds+leftover_candidates_budget;

            //nico ::supply_delta：当前市场流动资金增量
            rec.supply_delta = (rec.witness_budget + rec.worker_budget+rec.candidates_budget /*nico ::新增流出预算*/) -
                               (rec.leftover_worker_funds + rec.from_accumulated_fees + rec.from_unused_witness_budget+ rec.leftover_candidates_budget/*nico ::上期预算剩余部分以及手续费，回流资金池*/);

            modify(core, [&](asset_dynamic_data_object &_core) {
                  _core.current_supply = (_core.current_supply + rec.supply_delta);

                  assert(rec.supply_delta ==
                         witness_budget + worker_budget +rec.candidates_budget- 
                         leftover_worker_funds -
                          _core.accumulated_fees - dpo.witness_budget-rec.leftover_candidates_budget);
                  _core.accumulated_fees = 0;
            });

            modify(dpo, [&](dynamic_global_property_object &_dpo) {
                  // Since initial witness_budget was rolled into
                  // available_funds, we replace it with witness_budget
                  // instead of adding it.
                  _dpo.witness_budget = witness_budget;
                  _dpo.last_budget_time = now;
            });

            create<budget_record_object>([&](budget_record_object &_rec) {
                  _rec.time = head_block_time();
                  _rec.record = rec;
            });

            // available_funds is money we could spend, but don't want to.
            // we simply let it evaporate back into the reserve.
      }
      FC_CAPTURE_AND_RETHROW()
}

template <typename Visitor>
void visit_special_authorities(const database &db, Visitor visit)
{
      const auto &sa_idx = db.get_index_type<special_authority_index>().indices().get<by_id>();

      for (const special_authority_object &sao : sa_idx)
      {
            const account_object &acct = sao.account(db);
            if (!(acct.owner_special_authority.valid() && acct.active_special_authority.valid()) && acct.top_n_control_flags.valid())
                  return;
            if (acct.owner_special_authority->which() != special_authority::tag<no_special_authority>::value)
            {
                  visit(acct, true, *acct.owner_special_authority);
            }
            if (acct.active_special_authority->which() != special_authority::tag<no_special_authority>::value)
            {
                  visit(acct, false, *acct.active_special_authority);
            }
      }
}

void update_top_n_authorities(database &db)
{
      //访问特权权限
      visit_special_authorities(db,
                                [&](const account_object &acct, bool is_owner, const special_authority &auth) {
                                      if (auth.which() == special_authority::tag<top_holders_special_authority>::value)
                                      {
                                            // use index to grab the top N holders of the asset and vote_counter to obtain the weights

                                            const top_holders_special_authority &tha = auth.get<top_holders_special_authority>();
                                            vote_counter vc;
                                            const auto &bal_idx = db.get_index_type<account_balance_index>().indices().get<by_asset_balance>();
                                            uint8_t num_needed = tha.num_top_holders;
                                            if (num_needed == 0)
                                                  return;

                                            // find accounts
                                            const auto range = bal_idx.equal_range(boost::make_tuple(tha.asset));
                                            for (const account_balance_object &bal : boost::make_iterator_range(range.first, range.second))
                                            {
                                                  assert(bal.asset_type == tha.asset);

                                                  if (bal.owner == acct.id)
                                                        continue;

                                                  vc.add(bal.owner, bal.balance.value);
                                                  --num_needed;
                                                  if (num_needed == 0)
                                                        break;
                                            }

                                            db.modify(acct, [&](account_object &a) {
                                                  vc.finish(is_owner ? a.owner : a.active);
                                                  if (!vc.is_empty())
                                                        *a.top_n_control_flags |= (is_owner ? account_object::top_n_control_owner : account_object::top_n_control_active);
                                            });
                                      }
                                });
}

void deprecate_annual_members(database &db)
{
      const auto &account_idx = db.get_index_type<account_index>().indices().get<by_id>();
      fc::time_point_sec now = db.head_block_time();
      for (const account_object &acct : account_idx)
      {
            try
            {
                  transaction_evaluation_state upgrade_context(&db);
                  if (acct.is_annual_member(now))
                  {
                        account_upgrade_operation upgrade_vop;
                        upgrade_vop.account_to_upgrade = acct.id;
                        upgrade_vop.upgrade_to_lifetime_member = true;
                        db.apply_operation(upgrade_context, upgrade_vop);
                  }
            }
            catch (const fc::exception &e)
            {
                  // we can in fact get here, e.g. if asset issuer of buy/sell asset blacklists/whitelists the buyback account
                  wlog("Skipping annual member deprecate processing for account ${a} (${an}) at block ${n}; exception was ${e}",
                       ("a", acct.id)("an", acct.name)("n", db.head_block_num())("e", e.to_detail_string()));
                  continue;
            }
      }
      return;
}

void database::process_bids(const asset_bitasset_data_object &bad)
{
      /* 取消二元预测市场
      if (bad.is_prediction_market)
            return;
      */
      if (bad.current_feed.settlement_price.is_null())
            return;

      asset_id_type to_revive_id = (asset(0, bad.options.short_backing_asset) * bad.settlement_price).asset_id;
      const asset_object &to_revive = to_revive_id(*this);
      const asset_dynamic_data_object &bdd = to_revive.dynamic_data(*this);

      const auto &bid_idx = get_index_type<collateral_bid_index>().indices().get<by_price>();
      const auto start = bid_idx.lower_bound(boost::make_tuple(to_revive_id, price::max(bad.options.short_backing_asset, to_revive_id), collateral_bid_id_type()));

      share_type covered = 0;
      auto itr = start;
      while (covered < bdd.current_supply && itr != bid_idx.end() && itr->inv_swan_price.quote.asset_id == to_revive_id)
      {
            const collateral_bid_object &bid = *itr;
            asset total_collateral = bid.inv_swan_price.quote * bad.settlement_price;
            total_collateral += bid.inv_swan_price.base;
            price call_price = price::call_price(bid.inv_swan_price.quote, total_collateral, bad.current_feed.maintenance_collateral_ratio);
            if (~call_price >= bad.current_feed.settlement_price)
                  break;
            covered += bid.inv_swan_price.quote.amount;
            ++itr;
      }
      if (covered < bdd.current_supply)
            return;

      const auto end = itr;
      share_type to_cover = bdd.current_supply;
      share_type remaining_fund = bad.settlement_fund;
      for (itr = start; itr != end;)
      {
            const collateral_bid_object &bid = *itr;
            ++itr;
            share_type debt = bid.inv_swan_price.quote.amount;
            share_type collateral = (bid.inv_swan_price.quote * bad.settlement_price).amount;
            if (bid.inv_swan_price.quote.amount >= to_cover)
            {
                  debt = to_cover;
                  collateral = remaining_fund;
            }
            to_cover -= debt;
            remaining_fund -= collateral;
            execute_bid(bid, debt, collateral, bad.current_feed);
      }
      FC_ASSERT(remaining_fund == 0);
      FC_ASSERT(to_cover == 0);

      _cancel_bids_and_revive_mpa(to_revive, bad);
}

void database::perform_chain_maintenance(const signed_block &next_block, const global_property_object &global_props)
{
      const auto &gpo = get_global_properties();
      update_top_n_authorities(*this);
      update_active_witnesses();
      update_active_committee_members();
      modify(unsuccessful_candidates_id_type()(*this), [&](unsuccessful_candidates_object &unsuccessful_candidates) {
            unsuccessful_candidates.unsuccessful_candidates.clear();
            for (auto &candidate : vote_result)
                  if (!candidate.second)
                        unsuccessful_candidates.unsuccessful_candidates.insert(candidate.first);
            vote_result.clear();
      });
      auto old_gpo=gpo;
      modify(gpo, [this](global_property_object &p) {
            // Remove scaling of account registration fee
            const auto &dgpo = get_dynamic_global_properties();
            p.parameters.current_fees->get<account_create_operation>().basic_fee >>= p.parameters.account_fee_scale_bitshifts *
                                                                                     (dgpo.accounts_registered_this_interval / p.parameters.accounts_per_fee_scale);

            if (p.pending_parameters)
            {
                  p.parameters = std::move(*p.pending_parameters);
                  p.pending_parameters.reset();
            }
      });

      auto next_maintenance_time = get<dynamic_global_property_object>(dynamic_global_property_id_type()).next_maintenance_time;
      auto maintenance_interval = gpo.parameters.maintenance_interval;

      if (next_maintenance_time <= next_block.timestamp)
      {
            if (next_block.block_num() == 1)
                  next_maintenance_time = time_point_sec() +
                                          (((next_block.timestamp.sec_since_epoch() / maintenance_interval) + 1) * maintenance_interval);
            else
            {
                  // We want to find the smallest k such that next_maintenance_time + k * maintenance_interval > head_block_time()
                  // This implies k > ( head_block_time() - next_maintenance_time ) / maintenance_interval
                  //
                  // Let y be the right-hand side of this inequality, i.e.
                  // y = ( head_block_time() - next_maintenance_time ) / maintenance_interval
                  //
                  // and let the fractional part f be y-floor(y).  Clearly 0 <= f < 1.
                  // We can rewrite f = y-floor(y) as floor(y) = y-f.
                  //
                  // Clearly k = floor(y)+1 has k > y as desired.  Now we must
                  // show that this is the least such k, i.e. k-1 <= y.
                  //
                  // But k-1 = floor(y)+1-1 = floor(y) = y-f <= y.
                  // So this k suffices.
                  //
                  auto y = (head_block_time() - next_maintenance_time).to_seconds() / maintenance_interval;
                  next_maintenance_time += (y + 1) * maintenance_interval;
            }
      }

      const dynamic_global_property_object &dgpo = get_dynamic_global_properties();

      modify(dgpo, [next_maintenance_time](dynamic_global_property_object &d) {
            d.next_maintenance_time = next_maintenance_time;
            d.accounts_registered_this_interval = 0;
      });

      // Reset all BitAsset force settlement volumes to zero
      //nico log::在维护期扫描是否有需要清算的智能货币，并进行清算
      for (const auto &d : get_index_type<asset_bitasset_data_index>().indices())
      {
            modify(d, [](asset_bitasset_data_object &o) { o.force_settled_volume = 0; });
            if (d.has_settlement())
                  process_bids(d);
      }

      // process_budget needs to run at the bottom because
      //  it needs to know the next_maintenance_time
      process_budget(old_gpo);
}

} // namespace chain
} // namespace graphene
