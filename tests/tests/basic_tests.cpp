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

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/protocol.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/exceptions.hpp>

#include <graphene/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include <fc/crypto/hex.hpp>
#include "../common/database_fixture.hpp"

#include <algorithm>
#include <random>

using namespace graphene::chain;
using namespace graphene::db;

BOOST_FIXTURE_TEST_SUITE( basic_tests, database_fixture )

/**
 * Verify that names are RFC-1035 compliant https://tools.ietf.org/html/rfc1035
 * https://github.com/cryptonomex/graphene/issues/15
 */
BOOST_AUTO_TEST_CASE( valid_name_test )
{
   BOOST_CHECK( !is_valid_name( "a" ) );
   BOOST_CHECK( !is_valid_name( "A" ) );
   BOOST_CHECK( !is_valid_name( "0" ) );
   BOOST_CHECK( !is_valid_name( "." ) );
   BOOST_CHECK( !is_valid_name( "-" ) );

   BOOST_CHECK( !is_valid_name( "aa" ) );
   BOOST_CHECK( !is_valid_name( "aA" ) );
   BOOST_CHECK( !is_valid_name( "a0" ) );
   BOOST_CHECK( !is_valid_name( "a." ) );
   BOOST_CHECK( !is_valid_name( "a-" ) );

   BOOST_CHECK( is_valid_name( "aaaaaa" ) );
   BOOST_CHECK( !is_valid_name( "aaaAaaa" ) );
   BOOST_CHECK( is_valid_name( "aaa0aaa" ) );
   BOOST_CHECK( is_valid_name( "aaaaaa.aaaaaa" ) );
   BOOST_CHECK( is_valid_name( "aaa-aaa" ) );

   BOOST_CHECK( is_valid_name( "aaaaa0" ) );
   BOOST_CHECK( !is_valid_name( "aaaAA0" ) );
   BOOST_CHECK( is_valid_name( "aaa000" ) );
   BOOST_CHECK( !is_valid_name( "aaaaa.0" ) );
   BOOST_CHECK( is_valid_name( "aaaaa-0000" ) );

   BOOST_CHECK(  is_valid_name( "aaaaaa-bbbbbb-cccccc" ) );
   BOOST_CHECK(  is_valid_name( "aaa-bbb.cccccc" ) );

   BOOST_CHECK( !is_valid_name( "aaa,bbb-ccc" ) );
   BOOST_CHECK( !is_valid_name( "aaa_bbb-ccc" ) );
   BOOST_CHECK( !is_valid_name( "aaa-BBB-ccc" ) );

   BOOST_CHECK( !is_valid_name( "1aaa-bbb" ) );
   BOOST_CHECK( !is_valid_name( "-aaa-bbb-ccc" ) );
   BOOST_CHECK( !is_valid_name( ".aaa-bbb-ccc" ) );
   BOOST_CHECK( !is_valid_name( "/aaa-bbb-ccc" ) );

   BOOST_CHECK( !is_valid_name( "aaa-bbb-ccc-" ) );
   BOOST_CHECK( !is_valid_name( "aaa-bbb-ccc." ) );
   BOOST_CHECK( !is_valid_name( "aaa-bbb-ccc.." ) );
   BOOST_CHECK( !is_valid_name( "aaa-bbb-ccc/" ) );

   BOOST_CHECK( !is_valid_name( "aaa..bbb-ccc" ) );
   BOOST_CHECK( is_valid_name( "aaaaaa.bbbbbb-cccccc" ) );
   BOOST_CHECK( is_valid_name( "aaaaaa.bbbbbb.cccccc" ) );

   BOOST_CHECK(  is_valid_name( "aaaaaa--bbbbbb--cccccc" ) );
   BOOST_CHECK(  !is_valid_name( "xn--sandmnnchen-p8a.de" ) );
   BOOST_CHECK(  !is_valid_name( "xn--sandmnnchen-p8a.dex" ) );
   BOOST_CHECK(  !is_valid_name( "xn-sandmnnchen-p8a.de" ) );
   BOOST_CHECK(  !is_valid_name( "xn-sandmnnchen-p8a.dex" ) );

   BOOST_CHECK(  is_valid_name( "this-label-has-less-than-64-char.acters-63-to-be-really-precise" ) );
   BOOST_CHECK( !is_valid_name( "this-label-has-more-than-63-char.act.ers-64-to-be-really-precise" ) );
   BOOST_CHECK( !is_valid_name( "none.of.these.labels.has.more.than-63.chars--but.still.not.valid" ) );
}

BOOST_AUTO_TEST_CASE( valid_symbol_test )
{
   BOOST_CHECK( !is_valid_symbol( "A" ) );
   BOOST_CHECK( !is_valid_symbol( "a" ) );
   BOOST_CHECK( !is_valid_symbol( "0" ) );
   BOOST_CHECK( !is_valid_symbol( "." ) );

   BOOST_CHECK( !is_valid_symbol( "AA" ) );
   BOOST_CHECK( !is_valid_symbol( "Aa" ) );
   BOOST_CHECK( !is_valid_symbol( "A0" ) );
   BOOST_CHECK( !is_valid_symbol( "A." ) );

   BOOST_CHECK( is_valid_symbol( "AAA" ) );
   BOOST_CHECK( !is_valid_symbol( "AaA" ) );
   BOOST_CHECK( is_valid_symbol( "A0A" ) );
   BOOST_CHECK( is_valid_symbol( "A.A" ) );

   BOOST_CHECK( !is_valid_symbol( "A..A" ) );
   BOOST_CHECK( !is_valid_symbol( "A.A." ) );
   BOOST_CHECK( !is_valid_symbol( "A.A.A" ) );

   BOOST_CHECK( is_valid_symbol( "AAAAAAAAAAAAAAAA" ) );
   BOOST_CHECK( !is_valid_symbol( "AAAAAAAAAAAAAAAAA" ) );
   BOOST_CHECK( is_valid_symbol( "A.AAAAAAAAAAAAAA" ) );
   BOOST_CHECK( !is_valid_symbol( "A.AAAAAAAAAAAA.A" ) );

   BOOST_CHECK( is_valid_symbol( "AAA000AAA" ) );
}

BOOST_AUTO_TEST_CASE( price_test )
{
    auto price_max = []( uint32_t a, uint32_t b )
    {   return price::max( asset_id_type(a), asset_id_type(b) );   };
    auto price_min = []( uint32_t a, uint32_t b )
    {   return price::min( asset_id_type(a), asset_id_type(b) );   };

    BOOST_CHECK( price_max(0,1) > price_min(0,1) );
    BOOST_CHECK( price_max(1,0) > price_min(1,0) );
    BOOST_CHECK( price_max(0,1) >= price_min(0,1) );
    BOOST_CHECK( price_max(1,0) >= price_min(1,0) );
    BOOST_CHECK( price_max(0,1) >= price_max(0,1) );
    BOOST_CHECK( price_max(1,0) >= price_max(1,0) );
    BOOST_CHECK( price_min(0,1) < price_max(0,1) );
    BOOST_CHECK( price_min(1,0) < price_max(1,0) );
    BOOST_CHECK( price_min(0,1) <= price_max(0,1) );
    BOOST_CHECK( price_min(1,0) <= price_max(1,0) );
    BOOST_CHECK( price_min(0,1) <= price_min(0,1) );
    BOOST_CHECK( price_min(1,0) <= price_min(1,0) );
    BOOST_CHECK( price_min(1,0) != price_max(1,0) );
    BOOST_CHECK( ~price_max(0,1) != price_min(0,1) );
    BOOST_CHECK( ~price_min(0,1) != price_max(0,1) );
    BOOST_CHECK( ~price_max(0,1) == price_min(1,0) );
    BOOST_CHECK( ~price_min(0,1) == price_max(1,0) );
    BOOST_CHECK( ~price_max(0,1) < ~price_min(0,1) );
    BOOST_CHECK( ~price_max(0,1) <= ~price_min(0,1) );
    price a(asset(1), asset(2,asset_id_type(1)));
    price b(asset(2), asset(2,asset_id_type(1)));
    price c(asset(1), asset(2,asset_id_type(1)));
    BOOST_CHECK(a < b);
    BOOST_CHECK(b > a);
    BOOST_CHECK(a == c);
    BOOST_CHECK(!(b == c));

    price_feed dummy;
    dummy.maintenance_collateral_ratio = 1002;
    dummy.maximum_short_squeeze_ratio = 1234;
    dummy.settlement_price = price(asset(1000), asset(2000, asset_id_type(1)));
    price_feed dummy2 = dummy;
    BOOST_CHECK(dummy == dummy2);
}

BOOST_AUTO_TEST_CASE( memo_test )
{ 
   try 
   {
      memo_data m;
      auto sender = generate_private_key("1");
      auto receiver = generate_private_key("2");
      m.from = sender.get_public_key();
      m.to = receiver.get_public_key();
      m.set_message(sender, receiver.get_public_key(), "Hello, world!", 12345);

      decltype(fc::digest(m)) hash("8de72a07d093a589f574460deb19023b4aff354b561eb34590d9f4629f51dbf3");
      if( fc::digest(m) != hash )
      {
         // If this happens, notify the web guys that the memo serialization format changed.
         edump((m)(fc::digest(m)));
         BOOST_FAIL("Memo format has changed. Notify the web guys and update this test.");
      }
      BOOST_CHECK_EQUAL(m.get_message(receiver, sender.get_public_key()), "Hello, world!");
   } FC_LOG_AND_RETHROW() 
}

BOOST_AUTO_TEST_CASE( exceptions )
{
   GRAPHENE_CHECK_THROW(FC_THROW_EXCEPTION(balance_claim_invalid_claim_amount, "Etc"), balance_claim_invalid_claim_amount);
}

BOOST_AUTO_TEST_CASE( scaled_precision )
{
   const int64_t _k = 1000;
   const int64_t _m = _k*_k;
   const int64_t _g = _m*_k;
   const int64_t _t = _g*_k;
   const int64_t _p = _t*_k;
   const int64_t _e = _p*_k;

   BOOST_CHECK( asset::scaled_precision( 0) == share_type(   1   ) );
   BOOST_CHECK( asset::scaled_precision( 1) == share_type(  10   ) );
   BOOST_CHECK( asset::scaled_precision( 2) == share_type( 100   ) );
   BOOST_CHECK( asset::scaled_precision( 3) == share_type(   1*_k) );
   BOOST_CHECK( asset::scaled_precision( 4) == share_type(  10*_k) );
   BOOST_CHECK( asset::scaled_precision( 5) == share_type( 100*_k) );
   BOOST_CHECK( asset::scaled_precision( 6) == share_type(   1*_m) );
   BOOST_CHECK( asset::scaled_precision( 7) == share_type(  10*_m) );
   BOOST_CHECK( asset::scaled_precision( 8) == share_type( 100*_m) );
   BOOST_CHECK( asset::scaled_precision( 9) == share_type(   1*_g) );
   BOOST_CHECK( asset::scaled_precision(10) == share_type(  10*_g) );
   BOOST_CHECK( asset::scaled_precision(11) == share_type( 100*_g) );
   BOOST_CHECK( asset::scaled_precision(12) == share_type(   1*_t) );
   BOOST_CHECK( asset::scaled_precision(13) == share_type(  10*_t) );
   BOOST_CHECK( asset::scaled_precision(14) == share_type( 100*_t) );
   BOOST_CHECK( asset::scaled_precision(15) == share_type(   1*_p) );
   BOOST_CHECK( asset::scaled_precision(16) == share_type(  10*_p) );
   BOOST_CHECK( asset::scaled_precision(17) == share_type( 100*_p) );
   BOOST_CHECK( asset::scaled_precision(18) == share_type(   1*_e) );
   GRAPHENE_CHECK_THROW( asset::scaled_precision(19), fc::exception );
}

BOOST_AUTO_TEST_CASE( merkle_root )
{
   signed_block block;
   vector<processed_transaction> tx;
   vector<digest_type> t;
   const uint32_t num_tx = 10;

   for( uint32_t i=0; i<num_tx; i++ )
   {
      tx.emplace_back();
      tx.back().ref_block_prefix = i;
      t.push_back( tx.back().merkle_digest() );
   }

   auto c = []( const digest_type& digest ) -> checksum_type
   {   return checksum_type::hash( digest );   };
   
   auto d = []( const digest_type& left, const digest_type& right ) -> digest_type
   {   return digest_type::hash( std::make_pair( left, right ) );   };

   BOOST_CHECK( block.calculate_merkle_root() == checksum_type() );

   block.transactions.push_back( std::make_pair( tx[0].hash(),tx[0] ) );
   BOOST_CHECK( block.calculate_merkle_root() ==
      c(t[0])
      );

   digest_type dA, dB, dC, dD, dE, dI, dJ, dK, dM, dN, dO;

   /*
      A=d(0,1)
         / \ 
        0   1
   */

   dA = d(t[0], t[1]);

   block.transactions.push_back( std::make_pair( tx[1].hash(),tx[1] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dA) );

   /*
            I=d(A,B)
           /        \
      A=d(0,1)      B=2
         / \        /
        0   1      2
   */

   dB = t[2];
   dI = d(dA, dB);

   block.transactions.push_back( std::make_pair( tx[2].hash(),tx[2] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dI) );

   /*
          I=d(A,B)
           /    \
      A=d(0,1)   B=d(2,3)
         / \    /   \
        0   1  2     3
   */

   dB = d(t[2], t[3]);
   dI = d(dA, dB);

   block.transactions.push_back( std::make_pair( tx[3].hash(),tx[3] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dI) );

   /*
                     __M=d(I,J)__
                    /            \
            I=d(A,B)              J=C
           /        \            /
      A=d(0,1)   B=d(2,3)      C=4
         / \        / \        /
        0   1      2   3      4
   */

   dC = t[4];
   dJ = dC;
   dM = d(dI, dJ);

   block.transactions.push_back( std::make_pair( tx[4].hash(),tx[4] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dM) );

   /*
                     __M=d(I,J)__
                    /            \
            I=d(A,B)              J=C
           /        \            /
      A=d(0,1)   B=d(2,3)   C=d(4,5)
         / \        / \        / \
        0   1      2   3      4   5
   */

   dC = d(t[4], t[5]);
   dJ = dC;
   dM = d(dI, dJ);

   block.transactions.push_back( std::make_pair( tx[5].hash(),tx[5] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dM) );

   /*
                     __M=d(I,J)__
                    /            \
            I=d(A,B)              J=d(C,D)
           /        \            /        \
      A=d(0,1)   B=d(2,3)   C=d(4,5)      D=6
         / \        / \        / \        /
        0   1      2   3      4   5      6
   */

   dD = t[6];
   dJ = d(dC, dD);
   dM = d(dI, dJ);

   block.transactions.push_back( std::make_pair( tx[6].hash(),tx[6] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dM) );

   /*
                     __M=d(I,J)__
                    /            \
            I=d(A,B)              J=d(C,D)
           /        \            /        \
      A=d(0,1)   B=d(2,3)   C=d(4,5)   D=d(6,7)
         / \        / \        / \        / \
        0   1      2   3      4   5      6   7
   */

   dD = d(t[6], t[7]);
   dJ = d(dC, dD);
   dM = d(dI, dJ);

   block.transactions.push_back( std::make_pair( tx[7].hash(),tx[7] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dM) );

   /*
                                _____________O=d(M,N)______________
                               /                                   \   
                     __M=d(I,J)__                                  N=K
                    /            \                              /
            I=d(A,B)              J=d(C,D)                 K=E
           /        \            /        \            /
      A=d(0,1)   B=d(2,3)   C=d(4,5)   D=d(6,7)      E=8
         / \        / \        / \        / \        /
        0   1      2   3      4   5      6   7      8
   */

   dE = t[8];
   dK = dE;
   dN = dK;
   dO = d(dM, dN);

   block.transactions.push_back( std::make_pair( tx[8].hash(),tx[8] ) );
   BOOST_CHECK( block.calculate_merkle_root() == c(dO) );

   /*
                                _____________O=d(M,N)______________
                               /                                   \   
                     __M=d(I,J)__                                  N=K
                    /            \                              /
            I=d(A,B)              J=d(C,D)                 K=E
           /        \            /        \            /
      A=d(0,1)   B=d(2,3)   C=d(4,5)   D=d(6,7)   E=d(8,9)
         / \        / \        / \        / \        / \
        0   1      2   3      4   5      6   7      8   9
   */

   dE = d(t[8], t[9]);
   dK = dE;
   dN = dK;
   dO = d(dM, dN);

   block.transactions.push_back(std::make_pair( tx[9].hash(),tx[9] ));
   BOOST_CHECK( block.calculate_merkle_root() == c(dO) );
}

BOOST_AUTO_TEST_SUITE_END()
