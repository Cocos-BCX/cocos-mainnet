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

#include <graphene/chain/evaluator.hpp>

namespace graphene { namespace chain {

class vesting_balance_create_evaluator;
class vesting_balance_withdraw_evaluator;

class vesting_balance_create_evaluator : public evaluator<vesting_balance_create_evaluator>
{
    public:
        typedef vesting_balance_create_operation operation_type;

        void_result do_evaluate( const vesting_balance_create_operation& op );
        object_id_result do_apply( const vesting_balance_create_operation& op );
};

class vesting_balance_withdraw_evaluator : public evaluator<vesting_balance_withdraw_evaluator>
{
    public:
        typedef vesting_balance_withdraw_operation operation_type;

        void_result do_evaluate( const vesting_balance_withdraw_operation& op );
        void_result do_apply( const vesting_balance_withdraw_operation& op );
        void pay_fee_for_gas(const operation& op);
        asset calculate_fee( const operation& op, const price& core_exchange_rate = price::unit_price() )const;
};

} } // graphene::chain
