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
#include <graphene/chain/protocol/types.hpp>
#include <graphene/chain/protocol/address.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/base58.hpp>
#include <algorithm>

namespace graphene {
  namespace chain {
   address::address(){}

   address::address( const std::string& base58str )
   {
      std::string prefix( GRAPHENE_ADDRESS_PREFIX );
      FC_ASSERT( is_valid( base58str, prefix ), "${str}", ("str",base58str) );

      std::vector<char> v = fc::from_base58( base58str.substr( prefix.size() ) );
      memcpy( (char*)addr._hash, v.data(), std::min<size_t>( v.size()-4, sizeof( addr ) ) );
   }

   bool address::is_valid( const std::string& base58str, const std::string& prefix )
   {
      const size_t prefix_len = prefix.size();
      if( base58str.size() <= prefix_len )
          return false;
      if( base58str.substr( 0, prefix_len ) != prefix )
          return false;
      std::vector<char> v;
      try
      {
		     v = fc::from_base58( base58str.substr( prefix_len ) );
      }
      catch( const fc::parse_error_exception& e )
      {
        return false;
      }

      if( v.size() != sizeof( fc::ripemd160 ) + 4 )
          return false;

      const fc::ripemd160 checksum = fc::ripemd160::hash( v.data(), v.size() - 4 );
      if( memcmp( v.data() + 20, (char*)checksum._hash, 4 ) != 0 )
          return false;

      return true;
   }

   address::address( const fc::ecc::public_key& pub )
   {
       auto dat = pub.serialize();
       addr = fc::ripemd160::hash( fc::sha512::hash( dat.data, sizeof( dat ) ) );
   }

   address::address( const pts_address& ptsaddr )
   {
       addr = fc::ripemd160::hash( (char*)&ptsaddr, sizeof( ptsaddr ) );
   }

   address::address( const fc::ecc::public_key_data& pub )
   {
       addr = fc::ripemd160::hash( fc::sha512::hash( pub.data, sizeof( pub ) ) );
   }

   address::address( const graphene::chain::public_key_type& pub )
   {
       addr = fc::ripemd160::hash( fc::sha512::hash( pub.key_data.data, sizeof( pub.key_data ) ) );
   }

   address::operator std::string()const
   {
        fc::array<char,24> bin_addr;
        memcpy( (char*)&bin_addr, (char*)&addr, sizeof( addr ) );
        auto checksum = fc::ripemd160::hash( (char*)&addr, sizeof( addr ) );
        memcpy( ((char*)&bin_addr)+20, (char*)&checksum._hash[0], 4 );
        return GRAPHENE_ADDRESS_PREFIX + fc::to_base58( bin_addr.data, sizeof( bin_addr ) );
   }

} } // namespace graphene::chain

namespace fc
{
    void to_variant( const graphene::chain::address& var,  variant& vo )
    {
        vo = std::string(var);
    }
    void from_variant( const variant& var,  graphene::chain::address& vo )
    {
        vo = graphene::chain::address( var.as_string() );
    }
}
