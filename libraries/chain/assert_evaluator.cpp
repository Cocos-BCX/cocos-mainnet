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

#include <graphene/chain/assert_evaluator.hpp>
#include <graphene/chain/block_summary_object.hpp>
#include <graphene/chain/database.hpp>

#include <sstream>

namespace graphene { namespace chain {

struct predicate_evaluator
{
   typedef void result_type;
   const database& db;

   predicate_evaluator( const database& d ):db(d){}

   void operator()( const  account_name_eq_lit_predicate& p )const
   {
      FC_ASSERT( p.account_id(db).name == p.name );
   }
   void operator()( const  asset_symbol_eq_lit_predicate& p )const
   {
      FC_ASSERT( p.asset_id(db).symbol == p.symbol );
   }
   void operator()( const block_id_predicate& p )const
   {
      FC_ASSERT( block_summary_id_type( block_header::num_from_id( p.id ) & 0xffff )(db).block_id == p.id );
   }
};

void_result assert_evaluator::do_evaluate( const assert_operation& o )
{ try {
   const database& _db = db();
   uint32_t skip = _db.get_node_properties().skip_flags;
   auto max_predicate_opcode = _db.get_global_properties().parameters.max_predicate_opcode;

   if( skip & database::skip_assert_evaluation )
      return void_result();

   for( const auto& p : o.predicates )
   {
      FC_ASSERT( p.which() >= 0 );
      FC_ASSERT( unsigned(p.which()) < max_predicate_opcode );
      p.visit( predicate_evaluator( _db ) );
   }
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

void_result assert_evaluator::do_apply( const assert_operation& o )
{ try {
   // assert_operation is always a no-op
   return void_result();
} FC_CAPTURE_AND_RETHROW( (o) ) }

} } // graphene::chain
