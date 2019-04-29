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
#include <graphene/chain/protocol/protocol.hpp>

namespace graphene { namespace chain {

bool account_name_eq_lit_predicate::validate()const
{
   return is_valid_name( name );
}

bool asset_symbol_eq_lit_predicate::validate()const
{
   return is_valid_symbol( symbol );
}

struct predicate_validator
{
   typedef void result_type;

   template<typename T>
   void operator()( const T& p )const
   {
      p.validate();
   }
};

void assert_operation::validate()const
{
   FC_ASSERT( fee.amount >= share_type(0) );
   for( const auto& item : predicates )
      item.visit( predicate_validator() );
}

/**
 * The fee for assert operations is proportional to their size,
 * but cheaper than a data fee because they require no storage
 */
share_type  assert_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee * predicates.size();
}


} }  // namespace graphene::chain
