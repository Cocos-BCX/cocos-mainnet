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

   class create_nh_asset_evaluator : public evaluator<create_nh_asset_evaluator>
   {
      public:
         typedef create_nh_asset_operation operation_type;

         void_result do_evaluate( const    create_nh_asset_operation& o );
         object_id_result do_apply( const create_nh_asset_operation& o );
   };

  class relate_nh_asset_evaluator : public evaluator<relate_nh_asset_evaluator>
   {
      public:
         typedef relate_nh_asset_operation operation_type;

         void_result do_evaluate( const    operation_type& o );
         void_result do_apply( const operation_type& o );
   };

   class delete_nh_asset_evaluator : public evaluator<delete_nh_asset_evaluator>
   {
      public:
         typedef delete_nh_asset_operation operation_type;

         void_result do_evaluate( const    delete_nh_asset_operation& o );
         void_result do_apply( const delete_nh_asset_operation& o );
   };

   class transfer_nh_asset_evaluator : public evaluator<transfer_nh_asset_evaluator>
   {
      public:
         typedef transfer_nh_asset_operation operation_type;

         void_result do_evaluate( const    transfer_nh_asset_operation& o );
         void_result do_apply( const transfer_nh_asset_operation& o );
   };


} } // graphene::chain
