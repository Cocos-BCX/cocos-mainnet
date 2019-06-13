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
#include <graphene/chain/exceptions.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <graphene/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "../common/database_fixture.hpp"

using namespace graphene::chain;

BOOST_FIXTURE_TEST_SUITE( confidential_tests, database_fixture )
BOOST_AUTO_TEST_CASE( confidential_test )
{ try {
   ACTORS( (dan)(nathan) )
   const asset_object& core = asset_id_type()(db);

   transfer(account_id_type()(db), dan, core.amount(1000000));

   transfer_to_blind_operation to_blind;
   to_blind.amount = core.amount(1000);
   to_blind.from   = dan.id;

   auto owner1_key = fc::ecc::private_key::generate();
   auto owner1_pub = owner1_key.get_public_key();
   auto owner2_key = fc::ecc::private_key::generate();
   auto owner2_pub = owner2_key.get_public_key();
  
   blind_output out1, out2;
   out1.owner = authority( 1, public_key_type(owner1_pub), 1 );
   out2.owner = authority( 1, public_key_type(owner2_pub), 1 );


   auto InB1  = fc::sha256::hash("InB1");
   auto InB2  = fc::sha256::hash("InB2");
   auto nonce1 = fc::sha256::hash("nonce");
   auto nonce2 = fc::sha256::hash("nonce2");

   out1.commitment  = fc::ecc::blind(InB1,250);
   out1.range_proof = fc::ecc::range_proof_sign( 0, out1.commitment, InB1, nonce1, 0, 0, 250 );

   out2.commitment = fc::ecc::blind(InB2,750);
   out2.range_proof = fc::ecc::range_proof_sign( 0, out2.commitment, InB1, nonce2, 0, 0, 750 );

   to_blind.blinding_factor = fc::ecc::blind_sum( {InB1,InB2}, 2 );
   to_blind.outputs = {out2,out1};

   trx.operations = {to_blind};
   sign( trx,  dan_private_key  );
   db.push_transaction(trx);
   trx.signatures.clear();

   BOOST_TEST_MESSAGE( "Transfering from blind to blind with change address" );
   auto Out3B  = fc::sha256::hash("Out3B");
   auto Out4B  = fc::ecc::blind_sum( {InB2,Out3B}, 1 ); // add InB2 - Out3b
   blind_output out3, out4;
   out3.commitment = fc::ecc::blind(Out3B,300);
   out3.range_proof = fc::ecc::range_proof_sign( 0, out3.commitment, InB1, nonce1, 0, 0, 300 );
   out4.commitment = fc::ecc::blind(Out4B,750-300-10);
   out4.range_proof = fc::ecc::range_proof_sign( 0, out3.commitment, InB1, nonce1, 0, 0, 750-300-10 );


   blind_transfer_operation blind_tr;
   blind_tr.fee = core.amount(10);
   blind_tr.inputs.push_back( {out2.commitment, out2.owner} );
   blind_tr.outputs = {out3,out4};
   blind_tr.validate();
   trx.operations = {blind_tr};
   sign( trx,  owner2_key  );
   db.push_transaction(trx);

   BOOST_TEST_MESSAGE( "Attempting to double spend the same commitments" );
   blind_tr.fee = core.amount(11);

   Out4B  = fc::ecc::blind_sum( {InB2,Out3B}, 1 ); // add InB2 - Out3b
   out4.commitment = fc::ecc::blind(Out4B,750-300-11);
   auto out4_amount = 750-300-10;
   out4.range_proof = fc::ecc::range_proof_sign( 0, out3.commitment, InB1, nonce1, 0, 0, 750-300-11 );
   blind_tr.outputs = {out4,out3};
   trx.operations = {blind_tr};
   BOOST_REQUIRE_THROW( db.push_transaction(trx, ~0), graphene::chain::blind_transfer_unknown_commitment );


   BOOST_TEST_MESSAGE( "Transfering from blind to nathan public" );
   out4.commitment = fc::ecc::blind(Out4B,750-300-10);

   transfer_from_blind_operation from_blind;
   from_blind.fee = core.amount(10);
   from_blind.to  = nathan.id;
   from_blind.amount = core.amount( out4_amount - 10 );
   from_blind.blinding_factor = Out4B;
   from_blind.inputs.push_back( {out4.commitment, out4.owner} );
   trx.operations = {from_blind};
   trx.signatures.clear();
   db.push_transaction(trx);

   BOOST_REQUIRE_EQUAL( get_balance( nathan, core ), 750-300-10-10 );

} FC_LOG_AND_RETHROW() }



BOOST_AUTO_TEST_SUITE_END()
