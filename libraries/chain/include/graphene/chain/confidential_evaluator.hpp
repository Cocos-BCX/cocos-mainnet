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

struct transfer_to_blind_operation;
struct transfer_from_blind_operation;
struct blind_transfer_operation;

class transfer_to_blind_evaluator : public evaluator<transfer_to_blind_evaluator>
{
   public:
      typedef transfer_to_blind_operation operation_type;

      void_result do_evaluate( const transfer_to_blind_operation& o );
      void_result do_apply( const transfer_to_blind_operation& o ) ;

      virtual void pay_fee() override;
};

class transfer_from_blind_evaluator : public evaluator<transfer_from_blind_evaluator>
{
   public:
      typedef transfer_from_blind_operation operation_type;

      void_result do_evaluate( const transfer_from_blind_operation& o );
      void_result do_apply( const transfer_from_blind_operation& o ) ;

      virtual void pay_fee() override;
};

class blind_transfer_evaluator : public evaluator<blind_transfer_evaluator>
{
   public:
      typedef blind_transfer_operation operation_type;

      void_result do_evaluate( const blind_transfer_operation& o );
      void_result do_apply( const blind_transfer_operation& o ) ;

      virtual void pay_fee() override;
};

} } // namespace graphene::chain
