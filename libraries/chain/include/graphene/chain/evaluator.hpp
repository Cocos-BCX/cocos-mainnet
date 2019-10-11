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
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>
#include <graphene/chain/protocol/operations.hpp>
#include <fc/smart_ref_impl.hpp>

namespace graphene
{
namespace chain
{

class database;
struct signed_transaction;
class generic_evaluator;
class transaction_evaluation_state;
struct operation_fee_visitor
{
  typedef void result_type;
  operation_fee_visitor(){}
  operation_fee_visitor(vector<asset>& fees):fees(fees){}
  void add_fee(const asset & fee)
  {
    fees.push_back(fee);
  }
  template <typename operation_result_type>
  result_type operator()(operation_result_type &op_re)
  {
    op_re.fees=fees;
  }
  vector<asset> fees;
  
};
class generic_evaluator
{
  public:
    virtual ~generic_evaluator() {}

    virtual int get_type() const = 0;
    void pay_fee_for_result(operation_result &result)
    {
    }
    virtual operation_result start_evaluate(transaction_evaluation_state &eval_state, const operation &op, bool apply);

    /**
       * @note derived classes should ASSUME that the default validation that is
       * indepenent of chain state should be performed by op.validate() and should
       * not perform these extra checks.
       */
    virtual operation_result evaluate(const operation &op) = 0;
    virtual operation_result apply(const operation &op) = 0;

    /**
       * Routes the fee to where it needs to go.  The default implementation
       * routes the fee to the account_statistics_object of the fee_paying_account.
       *
       * Before pay_fee() is called, the fee is computed by prepare_fee() and has been
       * moved out of the fee_paying_account and (if paid in a non-CORE asset) converted
       * by the asset's fee pool.
       *
       * Therefore, when pay_fee() is called, the fee only exists in this->core_fee_paid.
       * So pay_fee() need only increment the receiving balance.
       */
    virtual void pay_fee();
    virtual void pay_fee_for_operation(const operation &o)=0;

    database &db() const;
    transaction_evaluation_state *trx_state;
    operation_result result;
    operation_fee_visitor fee_visitor;

    //void check_required_authorities(const operation& op);
  protected:
    /**
       * @brief Fetch objects relevant to fee payer and set pointer members
       * @param account_id Account which is paying the fee
       * @param fee The fee being paid. May be in assets other than core.
       *
       * This method verifies that the fee is valid and sets the object pointer members and the fee fields. It should
       * be called during do_evaluate.
       *
       * In particular, core_fee_paid field is set by prepare_fee().
       */
    void prepare_fee(const account_id_type& account_id,const operation &op);


    object_id_type get_relative_id(object_id_type rel_id) const;


    // the next two functions are helpers that allow template functions declared in this
    // header to call db() without including database.hpp, which would
    // cause a circular dependency
    asset calculate_fee_for_operation(const operation &op,const price& core_exchange_rate=price::unit_price())const;
    void db_adjust_balance(const account_id_type &fee_payer, asset fee_from_account);

    //asset fee_from_account;
    share_type core_fee_paid;
    const account_object *fee_paying_account = nullptr;
    //const account_statistics_object *fee_paying_account_statistics = nullptr;
    //const asset_object *fee_asset = nullptr;
    //const asset_dynamic_data_object *fee_asset_dyn_data = nullptr;
};

class op_evaluator
{
  public:
    virtual ~op_evaluator() {}
    virtual operation_result evaluate(transaction_evaluation_state &eval_state, const operation &op, bool apply) = 0;
};

template <typename T>
class op_evaluator_impl : public op_evaluator
{
  public:
    virtual operation_result evaluate(transaction_evaluation_state &eval_state, const operation &op, bool apply = true) override
    {
        T eval;
        return eval.start_evaluate(eval_state, op, apply); //  ing
    }
};

template <typename DerivedEvaluator>
class evaluator : public generic_evaluator
{
  public:
    virtual int get_type() const override { return operation::tag<typename DerivedEvaluator::operation_type>::value; }

    virtual operation_result evaluate(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this);                  //从通用类型转为指定的验证类型
        const auto &op = o.get<typename DerivedEvaluator::operation_type>(); //获取对应的operation参数
        prepare_fee(op.fee_payer(), op); 
        /////////////////////////////////////////////////////////////////////////////////////////////////////
        return eval->do_evaluate(op);
    }
    virtual operation_result apply(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this); // 从通用类型转为指定的验证类型
        const auto &op = o.get<typename DerivedEvaluator::operation_type>();

        operation_result result = eval->do_apply(op); //执行数据应用
        return result;
    }

    virtual void pay_fee_for_operation(const operation &o) final override
    {
        auto *eval = static_cast<DerivedEvaluator *>(this); // 从通用类型转为指定的验证类型
        eval->pay_fee();
    }
};
} // namespace chain
} // namespace graphene
