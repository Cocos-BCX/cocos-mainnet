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

#include <graphene/chain/protocol/operations.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

   class create_nh_asset_order_evaluator : public evaluator<create_nh_asset_order_evaluator>
   {
      public:
         typedef create_nh_asset_order_operation operation_type;

         void_result do_evaluate( const    create_nh_asset_order_operation& o );
         object_id_result do_apply( const create_nh_asset_order_operation& o );
   };

   class cancel_nh_asset_order_evaluator : public evaluator<cancel_nh_asset_order_evaluator>
   {
      public:
         typedef cancel_nh_asset_order_operation operation_type;

         void_result do_evaluate( const    cancel_nh_asset_order_operation& o );
         object_id_result do_apply( const cancel_nh_asset_order_operation& o );
   };

   class fill_nh_asset_order_evaluator : public evaluator<fill_nh_asset_order_evaluator>
   {
      public:
         typedef fill_nh_asset_order_operation operation_type;

         void_result do_evaluate( const    fill_nh_asset_order_operation& o );
         object_id_result do_apply( const fill_nh_asset_order_operation& o );
   };

} } // graphene::chain
