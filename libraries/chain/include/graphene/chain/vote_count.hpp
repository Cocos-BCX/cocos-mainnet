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

#include <graphene/chain/protocol/authority.hpp>

namespace graphene { namespace chain {

/**
 * Keep track of vote totals in internal authority object.  See #533.
 */
struct vote_counter
{
   template< typename Component >
   void add( Component who, uint64_t votes )
   {
      if( votes == 0 )
         return;
      assert( votes <= last_votes );
      last_votes = votes;
      if( bitshift == -1 )
         bitshift = std::max(int(boost::multiprecision::detail::find_msb( votes )) - 15, 0);
      uint64_t scaled_votes = std::max( votes >> bitshift, uint64_t(1) );
      assert( scaled_votes <= std::numeric_limits<weight_type>::max() );
      total_votes += scaled_votes;
      assert( total_votes <= std::numeric_limits<uint32_t>::max() );
      auth.add_authority( who, weight_type( scaled_votes ) );
   }

   /**
    * Write into out_auth, but only if we have at least one member.
    */
   void finish( authority& out_auth )
   {
      if( total_votes == 0 )
         return;
      assert( total_votes <= std::numeric_limits<uint32_t>::max() );
      uint32_t weight = uint32_t( total_votes );
      weight = (weight >> 1)+1;
      auth.weight_threshold = weight;
      out_auth = auth;
   }

   bool is_empty()const
   {
      return (total_votes == 0);
   }

   uint64_t last_votes = std::numeric_limits<uint64_t>::max();
   uint64_t total_votes = 0;
   int8_t bitshift = -1;
   authority auth;
};

} } // graphene::chain
