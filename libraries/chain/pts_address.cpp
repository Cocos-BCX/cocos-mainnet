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
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/pts_address.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/elliptic.hpp>
#include <fc/crypto/ripemd160.hpp>
#include <algorithm>

namespace graphene { namespace chain {

   pts_address::pts_address()
   {
      memset( addr.data, 0, sizeof(addr.data) );
   }

   pts_address::pts_address( const std::string& base58str )
   {
      std::vector<char> v = fc::from_base58( fc::string(base58str) );
      if( v.size() )
         memcpy( addr.data, v.data(), std::min<size_t>( v.size(), sizeof(addr) ) );

      if( !is_valid() )
      {
         FC_THROW_EXCEPTION( invalid_pts_address, "invalid pts_address ${a}", ("a", base58str) );
      }
   }

   pts_address::pts_address( const fc::ecc::public_key& pub, bool compressed, uint8_t version )
   {
       fc::sha256 sha2;
       if( compressed )
       {
           auto dat = pub.serialize();
           sha2     = fc::sha256::hash(dat.data, sizeof(dat) );
       }
       else
       {
           auto dat = pub.serialize_ecc_point();
           sha2     = fc::sha256::hash(dat.data, sizeof(dat) );
       }
       auto rep      = fc::ripemd160::hash((char*)&sha2,sizeof(sha2));
       addr.data[0]  = version;
       memcpy( addr.data+1, (char*)&rep, sizeof(rep) );
       auto check    = fc::sha256::hash( addr.data, sizeof(rep)+1 );
       check = fc::sha256::hash(check);
       memcpy( addr.data+1+sizeof(rep), (char*)&check, 4 );
   }

   /**
    *  Checks the address to verify it has a
    *  valid checksum
    */
   bool pts_address::is_valid()const
   {
       auto check    = fc::sha256::hash( addr.data, sizeof(fc::ripemd160)+1 );
       check = fc::sha256::hash(check);
       return memcmp( addr.data+1+sizeof(fc::ripemd160), (char*)&check, 4 ) == 0;
   }

   pts_address::operator std::string()const
   {
        return fc::to_base58( addr.data, sizeof(addr) );
   }

} } // namespace graphene

namespace fc
{
   void to_variant( const graphene::chain::pts_address& var,  variant& vo )
   {
        vo = std::string(var);
   }
   void from_variant( const variant& var,  graphene::chain::pts_address& vo )
   {
        vo = graphene::chain::pts_address( var.as_string() );
   }
}
