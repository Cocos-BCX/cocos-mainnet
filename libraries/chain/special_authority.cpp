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

#include <graphene/chain/protocol/special_authority.hpp>
#include <graphene/chain/database.hpp>

namespace graphene { namespace chain {

struct special_authority_validate_visitor
{
   typedef void result_type;

   void operator()( const no_special_authority& a ) {}

   void operator()( const top_holders_special_authority& a )
   {
      FC_ASSERT( a.num_top_holders > 0 );
   }
};

void validate_special_authority( const special_authority& a )
{
   special_authority_validate_visitor vtor;
   a.visit( vtor );
}

struct special_authority_evaluate_visitor
{
   typedef void result_type;

   special_authority_evaluate_visitor( const database& d ) : db(d) {}

   void operator()( const no_special_authority& a ) {}

   void operator()( const top_holders_special_authority& a )
   {
      a.asset(db);     // require asset to exist
   }

   const database& db;
};

void evaluate_special_authority( const database& db, const special_authority& a )
{
   special_authority_evaluate_visitor vtor( db );
   a.visit( vtor );
}

} } // graphene::chain
