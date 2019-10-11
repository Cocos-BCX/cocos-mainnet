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
    pay_fee_for_operation(op);
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

void generic_evaluator::prepare_fee(const account_id_type &account_id, const operation &op)
{
  database &d = db();
  if (!d.core)
    d.core = &asset_id_type()(d);
  if (!d.GAS)
    d.GAS = &GRAPHENE_ASSET_GAS(d);
  fee_paying_account = &account_id(d); //支付手续费的账户
  //////////////////////////////////////// 手续费预算是否足够//////////////////////////////////
  core_fee_paid = calculate_fee_for_operation(op).amount; //计算手续费
}

void generic_evaluator::pay_fee()
{
  try
  {
    auto &d = db();
    fee_visitor.fees.clear();
    FC_ASSERT(d.GAS->options.core_exchange_rate,"GAS->options.core_exchange_rate is null");
    if ((!trx_state->skip_fee) && core_fee_paid > 0)
    {
      const auto &total_gas = d.get_balance(*fee_paying_account, *d.GAS);
      asset require_gas(double(core_fee_paid.value) * (*d.GAS->options.core_exchange_rate).to_real(), d.GAS->id);
      if (total_gas >= require_gas)
      {
        fee_paying_account->pay_fee(d, require_gas);
        db_adjust_balance(fee_paying_account->id, -require_gas);
        fee_visitor.add_fee(require_gas);
        result.visit(fee_visitor);
      }
      else
      {
        asset require_core = asset();
        if (total_gas.amount.value > 0)
        {
          fee_paying_account->pay_fee(d, total_gas);
          db_adjust_balance(fee_paying_account->id, -total_gas);
          fee_visitor.add_fee(total_gas);
          require_core = (require_gas - total_gas) * (*d.GAS->options.core_exchange_rate);
        }
        else
        {
          require_core = core_fee_paid;
        }
        fee_paying_account->pay_fee(d, require_core);
        db_adjust_balance(fee_paying_account->id, -require_core);
        fee_visitor.add_fee(require_core);
        result.visit(fee_visitor);
      }
    }
  }
  FC_CAPTURE_AND_RETHROW()
}

asset generic_evaluator::calculate_fee_for_operation(const operation &op, const price &core_exchange_rate) const
{
  return db().current_fee_schedule().calculate_fee(op, core_exchange_rate); // ： 计算手续费
}
void generic_evaluator::db_adjust_balance(const account_id_type &fee_payer, asset fee_from_account)
{
  db().adjust_balance(fee_payer, fee_from_account);
}

} // namespace chain
} // namespace graphene
