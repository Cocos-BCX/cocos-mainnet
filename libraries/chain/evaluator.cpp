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
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>
#include <graphene/chain/is_authorized_asset.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/fba_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/market_evaluator.hpp>
#include <graphene/chain/contract_evaluator.hpp>
#include <graphene/chain/protocol/fee_schedule.hpp>
/***************************nico add************************/
#include <boost/thread/thread.hpp>
//#include <boost/exception_ptr.hpp>
/***************************************************/
#include <fc/uint128.hpp>

namespace graphene
{
namespace chain
{
database &generic_evaluator::db() const
{
  return trx_state->db();
}
struct operation_visiter_get_payer
{
  typedef account_id_type result_type;
  template <typename operation_type>
  result_type operator()(const operation_type &op)
  {
    return op.fee_payer();
  }
};

operation_result generic_evaluator::start_evaluate(transaction_evaluation_state &eval_state, const operation &op, bool apply)
{
  try
  {
    trx_state = &eval_state;
    bool _apply_transaction_is_success = false;
    uint16_t maximum_run_time_ratio = db().get_global_properties().parameters.maximum_run_time_ratio;
    auto magnification = 10000ll * std::min(maximum_run_time_ratio / GRAPHENE_1_PERCENT, 50); //单次op最大时间占比不能超过50%
    uint64_t block_interval = eval_state.db().block_interval() * magnification;
    operation_result result;
    fc::microseconds now, start = fc::time_point::now().time_since_epoch();
    if (trx_state->run_mode != transaction_apply_mode::apply_block_mode)
    {
      fc::exception_ptr exp;
      boost::thread apply_transaction_thread([&]() {
        try
        {
          result = this->evaluate(op); // ing
          if (apply)
            result = this->apply(op);
          int64_t runtime = (fc::time_point::now().time_since_epoch() - start).count();
          auto result_visitor = operation_result_visitor_set_runtime(runtime);
          result.visit(result_visitor);
          if (op.which() == operation::tag<call_contract_function_operation>::value && result.which() == operation_result::tag<contract_result>::value) //合约附加费用contract_result
          {
            static_cast<graphene::chain::call_contract_function_evaluator *>(this)->pay_fee_for_result(result.get<contract_result>());
            FC_ASSERT(core_fee_paid.value < db().get_global_properties().parameters.current_fees->maximun_handling_fee);
          }
        }
        catch (fc::exception &e)
        {
          exp = e.dynamic_copy_exception(); //nico add 多线程父子进程异常传递，1, 拷贝异常
        }
        _apply_transaction_is_success = true;
      });
      do
      {
        usleep(100); //主线程超时检测，检测间隔0.1毫秒,子线程平均执行时间为0.5毫秒
        now = fc::time_point::now().time_since_epoch();
      } while (now.count() - start.count() <= block_interval && (!_apply_transaction_is_success));
      if (exp)
      {
        apply_transaction_thread.interrupt();
        apply_transaction_thread.join();  //此处必须回收子线程，保证线程安全，才能正常重新抛出异常
        exp->dynamic_rethrow_exception(); //nico add 多线程父子进程异常传递，2, 子线程异常检测，重新抛出
      }
      if (!_apply_transaction_is_success)
      {
        apply_transaction_thread.interrupt();
        apply_transaction_thread.join();
        FC_THROW("The operation maybe apply timeout");
      }
    }
    else
    {
      result = this->evaluate(op); // ing
      if (apply)
        result = this->apply(op);
      int64_t runtime = (fc::time_point::now().time_since_epoch() - start).count();
      auto result_visitor = operation_result_visitor_set_runtime(runtime);
      result.visit(result_visitor);
      if (op.which() == operation::tag<call_contract_function_operation>::value) //合约附加费用contract_result
      {
        auto &temp_result = ((processed_transaction *)trx_state->_trx)->operation_results[db().get_current_op_index()];
        if (temp_result.which() == operation_result::tag<error_result>::value)
          throw result;
        FC_ASSERT(result.get<contract_result>().contract_affecteds == temp_result.get<contract_result>().contract_affecteds);
        result = temp_result;
        static_cast<graphene::chain::call_contract_function_evaluator *>(this)->pay_fee_for_result(result.get<contract_result>());
        FC_ASSERT(core_fee_paid.value < db().get_global_properties().parameters.current_fees->maximun_handling_fee);
      }
    }
    pay_fee_for_operation(op, result);
    return result;
  }
  FC_CAPTURE_AND_RETHROW()

  /************** BM 单线程方案****************#/
    try {
      trx_state   = &eval_state;
      //check_required_authorities(op);
      auto result =this->evaluate( op );   // ing  

      if( apply ) result = this->apply( op );
      return result;
    } FC_CAPTURE_AND_RETHROW() */
} // namespace chain

void generic_evaluator::prepare_fee(account_id_type account_id, asset fee)
{
  const database &d = db();
  fee_from_account = fee;
  FC_ASSERT(fee.amount >= share_type(0));
  fee_paying_account = &account_id(d); //支付手续费的账户
  fee_paying_account_statistics = &fee_paying_account->statistics(d);

  fee_asset = &fee.asset_id(d);                              //资产种类
  fee_asset_dyn_data = &fee_asset->dynamic_asset_data_id(d); //资产流通详情

  //if( d.head_block_time() > HARDFORK_419_TIME )
  //{
  //  验证是否有权使用对应的资产用于支付手续费，资产黑名单
  FC_ASSERT(is_authorized_asset(d, *fee_paying_account, *fee_asset), "Account ${acct} '${name}' attempted to pay fee by using asset ${a} '${sym}', which is unauthorized due to whitelist / blacklist",
            ("acct", fee_paying_account->id)("name", fee_paying_account->name)("a", fee_asset->id)("sym", fee_asset->symbol));
  //}

  if (fee_from_account.asset_id == asset_id_type()) //asset_id_type默认指向1.3.0 核心资产
    core_fee_paid = fee_from_account.amount;
  else
  { //如资产不是1.3.0核心资产，则执行核心汇率计算
    asset fee_from_pool = fee_from_account * fee_asset->options.core_exchange_rate;
    FC_ASSERT(fee_from_pool.asset_id == asset_id_type());
    core_fee_paid = fee_from_pool.amount;
    FC_ASSERT(core_fee_paid <= fee_asset_dyn_data->fee_pool, "Fee pool balance of '${b}' is less than the ${r} required to convert ${c}",
              ("r", db().to_pretty_string(fee_from_pool))("b", db().to_pretty_string(fee_asset_dyn_data->fee_pool))("c", db().to_pretty_string(fee)));
  }
}

void generic_evaluator::convert_fee()
{
  if (!trx_state->skip_fee)
  {
    if (fee_asset->get_id() != asset_id_type())
    {
      db().modify(*fee_asset_dyn_data, [this](asset_dynamic_data_object &d) {
        d.accumulated_fees += fee_from_account.amount;
        d.fee_pool -= core_fee_paid;
      });
    }
  }
}

void generic_evaluator::pay_fee()
{
  try
  {
    if (!trx_state->skip_fee)
    {
      database &d = db();
      /// TODO: db().pay_fee( account_id, core_fee );
      d.modify(*fee_paying_account_statistics, [&](account_statistics_object &s) {
        s.pay_fee(core_fee_paid, d.get_global_properties().parameters.cashback_vesting_threshold);
      });
    }
  }
  FC_CAPTURE_AND_RETHROW()
}

void generic_evaluator::pay_fba_fee(uint64_t fba_id)
{
  database &d = db();
  const fba_accumulator_object &fba = d.get<fba_accumulator_object>(fba_accumulator_id_type(fba_id));
  if (fba.is_configured(d))
  {
    d.modify(fba, [&](fba_accumulator_object &_fba) {
      _fba.accumulated_fba_fees += core_fee_paid;
    });
  }
  else
    generic_evaluator::pay_fee();
}

share_type generic_evaluator::calculate_fee_for_operation(const operation &op) const
{
  return db().current_fee_schedule().calculate_fee(op).amount; // ： 计算手续费
}
void generic_evaluator::db_adjust_balance(const account_id_type &fee_payer, asset fee_from_account)
{
  db().adjust_balance(fee_payer, fee_from_account);
}

} // namespace chain
} // namespace graphene
