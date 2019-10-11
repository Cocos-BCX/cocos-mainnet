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
#include <graphene/chain/protocol/asset.hpp>
#include <boost/rational.hpp>
#include <boost/multiprecision/cpp_int.hpp>

namespace graphene { namespace chain {
      typedef boost::multiprecision::uint128_t uint128_t;
      typedef boost::multiprecision::int128_t  int128_t;

      bool operator == ( const price& a, const price& b )
      {
         if( std::tie( a.base.asset_id, a.quote.asset_id ) != std::tie( b.base.asset_id, b.quote.asset_id ) )
             return false;

         const auto amult = uint128_t( b.quote.amount.value ) * a.base.amount.value;
         const auto bmult = uint128_t( a.quote.amount.value ) * b.base.amount.value;

         return amult == bmult;
      }

      bool operator < ( const price& a, const price& b )
      {
         if( a.base.asset_id < b.base.asset_id ) return true;
         if( a.base.asset_id > b.base.asset_id ) return false;
         if( a.quote.asset_id < b.quote.asset_id ) return true;
         if( a.quote.asset_id > b.quote.asset_id ) return false;

         const auto amult = uint128_t( b.quote.amount.value ) * a.base.amount.value;
         const auto bmult = uint128_t( a.quote.amount.value ) * b.base.amount.value;

         return amult < bmult;
      }

      bool operator <= ( const price& a, const price& b )
      {
         return (a == b) || (a < b);
      }

      bool operator != ( const price& a, const price& b )
      {
         return !(a == b);
      }

      bool operator > ( const price& a, const price& b )
      {
         return !(a <= b);
      }

      bool operator >= ( const price& a, const price& b )
      {
         return !(a < b);
      }

      asset operator * ( const asset& a, const price& b )
      {
         if( a.asset_id == b.base.asset_id )
         {
            FC_ASSERT( b.base.amount.value > 0 );
            uint128_t result = (uint128_t(a.amount.value) * b.quote.amount.value)/b.base.amount.value;
            FC_ASSERT( result <= GRAPHENE_MAX_SHARE_SUPPLY );
            return asset( result.convert_to<int64_t>(), b.quote.asset_id );
         }
         else if( a.asset_id == b.quote.asset_id )
         {
            FC_ASSERT( b.quote.amount.value > 0 );
            uint128_t result = (uint128_t(a.amount.value) * b.base.amount.value)/b.quote.amount.value;
            FC_ASSERT( result <= GRAPHENE_MAX_SHARE_SUPPLY );
            return asset( result.convert_to<int64_t>(), b.base.asset_id );
         }
         FC_THROW_EXCEPTION( fc::assert_exception, "invalid asset * price", ("asset",a)("price",b) );
      }

      price operator / ( const asset& base, const asset& quote )
      { try {
         FC_ASSERT( base.asset_id != quote.asset_id );
         return price{base,quote};
      } FC_CAPTURE_AND_RETHROW( (base)(quote) ) }

      price price::max( asset_id_type base, asset_id_type quote ) { return asset( share_type(GRAPHENE_MAX_SHARE_SUPPLY), base ) / asset( share_type(1), quote); }
      price price::min( asset_id_type base, asset_id_type quote ) { return asset( 1, base ) / asset( GRAPHENE_MAX_SHARE_SUPPLY, quote); }

      /**
       *  The black swan price is defined as debt/collateral, we want to perform a margin call
       *  before debt == collateral.   Given a debt/collateral ratio of 1 USD / CORE and
       *  a maintenance collateral requirement of 2x we can define the call price to be
       *  2 USD / CORE.
       *
       *  This method divides the collateral by the maintenance collateral ratio to derive
       *  a call price for the given black swan ratio.
       *
       *  There exists some cases where the debt and collateral values are so small that
       *  dividing by the collateral ratio will result in a 0 price or really poor
       *  rounding errors.   No matter what the collateral part of the price ratio can
       *  never go to 0 and the debt can never go more than GRAPHENE_MAX_SHARE_SUPPLY
       *
       *  CR * DEBT/COLLAT or DEBT/(COLLAT/CR)
       */
      price price::call_price( const asset& debt, const asset& collateral, uint16_t collateral_ratio)
      { try {
         //wdump((debt)(collateral)(collateral_ratio));
         boost::rational<int128_t> swan(debt.amount.value,collateral.amount.value);
         boost::rational<int128_t> ratio( collateral_ratio, GRAPHENE_COLLATERAL_RATIO_DENOM );
         auto cp = swan * ratio;

         while( cp.numerator() > GRAPHENE_MAX_SHARE_SUPPLY || cp.denominator() > GRAPHENE_MAX_SHARE_SUPPLY )
            cp = boost::rational<int128_t>( (cp.numerator() >> 1)+1, (cp.denominator() >> 1)+1 );

         return ~(asset( cp.numerator().convert_to<int64_t>(), debt.asset_id ) / asset( cp.denominator().convert_to<int64_t>(), collateral.asset_id ));
      } FC_CAPTURE_AND_RETHROW( (debt)(collateral)(collateral_ratio) ) }

      bool price::is_null() const { return *this == price(); }

      void price::validate() const
      { try {
         FC_ASSERT( base.amount > share_type(0) );
         FC_ASSERT( quote.amount > share_type(0) );
         FC_ASSERT( base.asset_id != quote.asset_id );
      } FC_CAPTURE_AND_RETHROW( (base)(quote) ) }

      void price_feed::validate() const
      { try {
         if( !settlement_price.is_null() )
            settlement_price.validate();
         FC_ASSERT( maximum_short_squeeze_ratio >= GRAPHENE_MIN_COLLATERAL_RATIO );
         FC_ASSERT( maximum_short_squeeze_ratio <= GRAPHENE_MAX_COLLATERAL_RATIO );
         FC_ASSERT( maintenance_collateral_ratio >= GRAPHENE_MIN_COLLATERAL_RATIO );
         FC_ASSERT( maintenance_collateral_ratio <= GRAPHENE_MAX_COLLATERAL_RATIO );
         FC_ASSERT( maintenance_collateral_ratio >= maximum_short_squeeze_ratio );
         max_short_squeeze_price(); // make sure that it doesn't overflow
      } FC_CAPTURE_AND_RETHROW( (*this) ) }

      bool price_feed::is_for( asset_id_type asset_id ) const
      {
         try
         {
            if( !settlement_price.is_null() )
               return (settlement_price.base.asset_id == asset_id);
            return true;
         }
         FC_CAPTURE_AND_RETHROW( (*this) )
      }

      price price_feed::max_short_squeeze_price()const
      {
         boost::rational<int128_t> sp( settlement_price.base.amount.value, settlement_price.quote.amount.value ); //debt.amount.value,collateral.amount.value);
         boost::rational<int128_t> ratio( GRAPHENE_COLLATERAL_RATIO_DENOM, maximum_short_squeeze_ratio );
         auto cp = sp * ratio;

         while( cp.numerator() > GRAPHENE_MAX_SHARE_SUPPLY || cp.denominator() > GRAPHENE_MAX_SHARE_SUPPLY )
            cp = boost::rational<int128_t>( (cp.numerator() >> 1)+(cp.numerator()&1), (cp.denominator() >> 1)+(cp.denominator()&1) );

         return (asset( cp.numerator().convert_to<int64_t>(), settlement_price.base.asset_id ) / asset( cp.denominator().convert_to<int64_t>(), settlement_price.quote.asset_id ));
      }

// compile-time table of powers of 10 using template metaprogramming

template< int N >
struct p10
{
   static const int64_t v = 10 * p10<N-1>::v;
};

template<>
struct p10<0>
{
   static const int64_t v = 1;
};

const int64_t scaled_precision_lut[19] =
{
   p10<  0 >::v, p10<  1 >::v, p10<  2 >::v, p10<  3 >::v,
   p10<  4 >::v, p10<  5 >::v, p10<  6 >::v, p10<  7 >::v,
   p10<  8 >::v, p10<  9 >::v, p10< 10 >::v, p10< 11 >::v,
   p10< 12 >::v, p10< 13 >::v, p10< 14 >::v, p10< 15 >::v,
   p10< 16 >::v, p10< 17 >::v, p10< 18 >::v
};

} } // graphene::chain
