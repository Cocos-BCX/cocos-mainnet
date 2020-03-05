/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, includingss without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUdingss BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <fc/smart_ref_impl.hpp>
#include <fc/uint128.hpp>

#include <graphene/chain/hardfork.hpp>

// #include <graphene/chain/fba_accumulator_id.hpp>

// #include <graphene/chain/fba_object.hpp>
#include <graphene/chain/market_object.hpp>
#include <graphene/chain/proposal_object.hpp>
#include <graphene/chain/vesting_balance_object.hpp>
#include <graphene/chain/exceptions.hpp>

#include <boost/test/unit_test.hpp>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE( fee_tests, database_fixture )

BOOST_AUTO_TEST_CASE( nonzero_fee_test )
{
   try
   {
      ACTORS((antelope)(zebras));

      const share_type prec = asset::scaled_precision( asset_id_type()(*db).precision );

      // Return number of core shares (times precision)
      auto _core = [&]( int64_t x ) -> asset
      {  return asset( x*prec );    };

      transfer( committee_account, antelope_id, _core(1000000) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      signed_transaction tx;
      transfer_operation xfer_op;
      xfer_op.from = antelope_id;
      xfer_op.to = zebras_id;
      xfer_op.amount = _core(1000);
      tx.operations.push_back( xfer_op );
      set_expiration( db.get(), tx );
      sign( tx, antelope_private_key );
    //   GRAPHENE_REQUIRE_THROW( PUSH_TX( db.get(), tx ), insufficient_fee );  // unknown exception
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}

// BOOST_AUTO_TEST_CASE(asset_claim_fees_test)
// {
//    try
//    {
//       ACTORS((antelopes)(zebras)(mustang)(alpaca));
//       // mustang issues asset to antelopes
//       // alpaca issues asset to zebras
//       // antelopes and zebras trade in the market and pay fees
//       // Verify that mustang and alpaca can claim the fees

//       const share_type core_prec = asset::scaled_precision( asset_id_type()(*db).precision );

//       // Return number of core shares (times precision)
//       auto _core = [&]( int64_t x ) -> asset
//       {  return asset( x*core_prec );    };

//       transfer( committee_account, antelopes_id, _core(1000000) );
//       transfer( committee_account,   zebras_id, _core(1000000) );
//       transfer( committee_account,  mustang_id, _core(1000000) );
//       transfer( committee_account,  alpaca_id, _core(1000000) );

//       asset_id_type mustangcoin_id = create_bitasset( "MUSTCOIN", mustang_id,   GRAPHENE_1_PERCENT, charge_market_fee ).id;
//       asset_id_type alpacacoin_id = create_bitasset( "ALPACOIN", alpaca_id, 2*GRAPHENE_1_PERCENT, charge_market_fee ).id;

//       const share_type mustang_prec = asset::scaled_precision( asset_id_type(mustangcoin_id)(*db).precision );
//       const share_type alpaca_prec = asset::scaled_precision( asset_id_type(alpacacoin_id)(*db).precision );

//       auto _mustang = [&]( int64_t x ) -> asset
//       {   return asset( x*mustang_prec, mustangcoin_id );   };
//       auto _alpaca = [&]( int64_t x ) -> asset
//       {   return asset( x*alpaca_prec, alpacacoin_id );   };

//       update_feed_producers( mustangcoin_id(*db), { mustang_id } );
//       update_feed_producers( alpacacoin_id(*db), { alpaca_id } );

//       const asset mustang_satoshi = asset(1, mustangcoin_id);
//       const asset alpaca_satoshi = asset(1, alpacacoin_id);

//       // mustangcoin is worth 100 BTS
//       price_feed feed;
//       feed.settlement_price = price( _mustang(1), _core(100) );
//       feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
//       feed.maximum_short_squeeze_ratio = 150 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
//       publish_feed( mustangcoin_id(*db), mustang, feed );

//       // alpacacoin is worth 30 BTS
//       feed.settlement_price = price( _alpaca(1), _core(30) );
//       feed.maintenance_collateral_ratio = 175 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
//       feed.maximum_short_squeeze_ratio = 150 * GRAPHENE_COLLATERAL_RATIO_DENOM / 100;
//       publish_feed( alpacacoin_id(*db), alpaca, feed );

//       enable_fees();

//       // antelopes and zebras create some coins
//       borrow( antelopes_id, _mustang( 200), _core( 60000) );
//       borrow(   zebras_id, _alpaca(2000), _core(180000) );

//       // antelopes and zebras place orders which match
//       create_sell_order( antelopes_id, _mustang(100), _alpaca(300) );   // antelopes is willing to sell her mustang's for 3 alpaca
//       create_sell_order(   zebras_id, _alpaca(700), _mustang(200) );   // zebras is buying up to 200 mustang's for up to 3.5 alpaca

//       // 100 mustangs and 300 alpacas are matched, so the fees should be
//       //  1 mustang (1%) and 6 alpaca (2%).

//       auto claim_fees = [&]( account_id_type issuer, asset amount_to_claim )
//       {
//          asset_claim_fees_operation claim_op;
//          claim_op.issuer = issuer;
//          claim_op.amount_to_claim = amount_to_claim;
//          signed_transaction tx;
//          tx.operations.push_back( claim_op );
//         //  db.current_fee_schedule().set_fee( tx.operations.back() );
//          set_expiration( db.get(), tx );
//          fc::ecc::private_key   my_pk = (issuer == mustang_id) ? mustang_private_key : alpaca_private_key;
//          fc::ecc::private_key your_pk = (issuer == mustang_id) ? alpaca_private_key : mustang_private_key;
//          sign( tx, your_pk );
//          GRAPHENE_REQUIRE_THROW( PUSH_TX( db.get(), tx ), fc::exception );
//          tx.signatures.clear();
//          sign( tx, my_pk );
//          PUSH_TX( db.get(), tx );
//       };

//       {
//          const asset_object& mustangcoin = mustangcoin_id(*db);
//          const asset_object& alpacacoin = alpacacoin_id(*db);

//          //wdump( (mustangcoin)(mustangcoin.dynamic_asset_data_id(*db))((*mustangcoin.bitasset_data_id)(*db)) );
//          //wdump( (alpacacoin)(alpacacoin.dynamic_asset_data_id(*db))((*alpacacoin.bitasset_data_id)(*db)) );

//          // check the correct amount of fees has been awarded
//          BOOST_CHECK( mustangcoin.dynamic_asset_data_id(*db).accumulated_fees == _mustang(1).amount );
//          BOOST_CHECK( alpacacoin.dynamic_asset_data_id(*db).accumulated_fees == _alpaca(6).amount );

//       }
// /*
//       if( db.head_block_time() <= HARDFORK_413_TIME )
//       {
//          // can't claim before hardfork
//          GRAPHENE_REQUIRE_THROW( claim_fees( mustang_id, _mustang(1) ), fc::exception );
//          generate_blocks( HARDFORK_413_TIME );
//          while( db.head_block_time() <= HARDFORK_413_TIME )
//          {
//             generate_block();
//          }
//       }
// */
//       {
//          const asset_object& mustangcoin = mustangcoin_id(*db);
//          const asset_object& alpacacoin = alpacacoin_id(*db);

//          // can't claim more than balance
//          GRAPHENE_REQUIRE_THROW( claim_fees( mustang_id, _mustang(1) + mustang_satoshi ), fc::exception );
//          GRAPHENE_REQUIRE_THROW( claim_fees( alpaca_id, _alpaca(6) + alpaca_satoshi ), fc::exception );

//          // can't claim asset that doesn't belong to you
//          GRAPHENE_REQUIRE_THROW( claim_fees( alpaca_id, mustang_satoshi ), fc::exception );
//          GRAPHENE_REQUIRE_THROW( claim_fees( mustang_id, alpaca_satoshi ), fc::exception );

//          // can claim asset in one go
//          claim_fees( mustang_id, _mustang(1) );
//          GRAPHENE_REQUIRE_THROW( claim_fees( mustang_id, mustang_satoshi ), fc::exception );
//          BOOST_CHECK( mustangcoin.dynamic_asset_data_id(*db).accumulated_fees == _mustang(0).amount );

//          // can claim in multiple goes
//          claim_fees( alpaca_id, _alpaca(4) );
//          BOOST_CHECK( alpacacoin.dynamic_asset_data_id(*db).accumulated_fees == _alpaca(2).amount );
//          GRAPHENE_REQUIRE_THROW( claim_fees( alpaca_id, _alpaca(2) + alpaca_satoshi ), fc::exception );
//          claim_fees( alpaca_id, _alpaca(2) );
//          BOOST_CHECK( alpacacoin.dynamic_asset_data_id(*db).accumulated_fees == _alpaca(0).amount );
//       }
//    } FC_LOG_AND_RETHROW()
// }

///////////////////////////////////////////////////////////////
// cashback_test infrastructure                              //
///////////////////////////////////////////////////////////////

#define CHECK_BALANCE( actor_name, amount ) \
   BOOST_CHECK_EQUAL( get_balance( actor_name ## _id, asset_id_type() ), amount )

// #define CHECK_VESTED_CASHBACK( actor_name, amount ) \
//    BOOST_CHECK_EQUAL( actor_name ## _id(*db).statistics(*db).pendingss_vested_fees.value, amount )

// #define CHECK_UNVESTED_CASHBACK( actor_name, amount ) \
//    BOOST_CHECK_EQUAL( actor_name ## _id(*db).statistics(*db).pendingss_fees.value, amount )

#define GET_CASHBACK_BALANCE( account ) \
   ( (account.cashback_vb.valid()) \
   ? account.cashback_balance(*db).balance.amount.value \
   : 0 )

#define CHECK_CASHBACK_VBO( actor_name, _amount ) \
   BOOST_CHECK_EQUAL( GET_CASHBACK_BALANCE( actor_name ## _id(*db) ), _amount )

#define P100 GRAPHENE_100_PERCENT
#define P1 GRAPHENE_1_PERCENT

uint64_t pct( uint64_t percentage, uint64_t val )
{
   fc::uint128_t x = percentage;
   x *= val;
   x /= GRAPHENE_100_PERCENT;
   return x.to_uint64();
}

uint64_t pct( uint64_t percentage0, uint64_t percentage1, uint64_t val )
{
   return pct( percentage1, pct( percentage0, val ) );
}

uint64_t pct( uint64_t percentage0, uint64_t percentage1, uint64_t percentage2, uint64_t val )
{
   return pct( percentage2, pct( percentage1, pct( percentage0, val ) ) );
}

struct actor_audit
{
   int64_t b0 = 0;      // starting balance parameter
   int64_t bal = 0;     // balance should be this
   int64_t ubal = 0;    // unvested balance (in VBO) should be this
   int64_t ucb = 0;     // unvested cashback in account_statistics should be this
   int64_t vcb = 0;     // vested cashback in account_statistics should be this
   int64_t ref_pct = 0; // referrer percentage should be this
};

// BOOST_AUTO_TEST_CASE( cashback_test )
// { try {
//    /*                        Account Structure used in this test                         *
//     *                                                                                    *
//     *               /-----------------\       /-------------------\                      *
//     *               | life (Lifetime) |       |  rog (Lifetime)   |                      *
//     *               \-----------------/       \-------------------/                      *
//     *                  | Ref&Reg    | Refers     | Registers  | Registers                *
//     *                  |            | 75         | 25         |                          *
//     *                  v            v            v            |                          *
//     *  /----------------\         /----------------\          |                          *
//     *  |  ann (Annual)  |         |  dumy (basic)  |          |                          *
//     *  \----------------/         \----------------/          |-------------.            *
//     * 80 | Refers      L--------------------------------.     |             |            *
//     *    v                     Refers                80 v     v 20          |            *
//     *  /----------------\                         /----------------\        |            *
//     *  |  scud (basic)  |<------------------------|  stud (basic)  |        |            *
//     *  \----------------/ 20   Registers          | (Upgrades to   |        | 5          *
//     *                                             |   Lifetime)    |        v            *
//     *                                             \----------------/   /--------------\  *
//     *                                                         L------->| pleb (Basic) |  *
//     *                                                       95 Refers  \--------------/  *
//     *                                                                                    *
//     * Fee distribution chains (80-20 referral/net split, 50-30 referrer/LTM split)       *
//     * life : 80% -> life, 20% -> net                                                     *
//     * rog: 80% -> rog, 20% -> net                                                        *
//     * ann (before upg): 80% -> life, 20% -> net                                          *
//     * ann (after upg): 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% -> net                   *
//     * stud (before upg): 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% * 80% -> rog,          *
//     *                    20% -> net                                                      *
//     * stud (after upg): 80% -> stud, 20% -> net                                          *
//     * dumy : 75% * 80% -> life, 25% * 80% -> rog, 20% -> net                             *
//     * scud : 80% * 5/8 -> ann, 80% * 3/8 -> life, 20% * 80% -> stud, 20% -> net          *
//     * pleb : 95% * 80% -> stud, 5% * 80% -> rog, 20% -> net                              *
//     */

//    BOOST_TEST_MESSAGE("Creating actors");

//    ACTOR(life);
//    ACTOR(rog);
   
//    PREP_ACTOR(ann);
//    PREP_ACTOR(scud);
//    PREP_ACTOR(dumy);
//    PREP_ACTOR(stud);
//    PREP_ACTOR(pleb);
   
//    account_id_type ann_id, scud_id, dumy_id, stud_id, pleb_id;
//    actor_audit alife, arog, aann, ascud, adumy, astud, apleb;

//    alife.b0 = 100000000;
//    arog.b0 = 100000000;
//    aann.b0 = 1000000;
//    astud.b0 = 1000000;
//    astud.ref_pct = 80 * GRAPHENE_1_PERCENT;
//    ascud.ref_pct = 80 * GRAPHENE_1_PERCENT;
//    adumy.ref_pct = 75 * GRAPHENE_1_PERCENT;
//    apleb.ref_pct = 95 * GRAPHENE_1_PERCENT;

//    transfer(account_id_type(), life_id, asset(alife.b0));
//    alife.bal += alife.b0;
//    transfer(account_id_type(), rog_id, asset(arog.b0));
//    arog.bal += arog.b0;
//    upgrade_to_lifetime_member(life_id);
//    upgrade_to_lifetime_member(rog_id);

//    BOOST_TEST_MESSAGE("Enable fees");
//    const auto& fees = db->get_global_properties().parameters.current_fees;

// #define CustomRegisterActor(actor_name, registrar_name, referrer_name, referrer_rate) \
//    { \
//       account_create_operation op; \
//       op.registrar = registrar_name ## _id; \
//       op.name = BOOST_PP_STRINGIZE(actor_name); \
//       op.options.memo_key = actor_name ## _private_key.get_public_key(); \
//       op.active = authority(1, public_key_type(actor_name ## _private_key.get_public_key()), 1); \
//       op.owner = op.active; \
//       trx.operations = {op}; \
//       sign( trx,  registrar_name ## _private_key ); \
//       actor_name ## _id = PUSH_TX( db.get(), trx ).operation_results.front().get<object_id_result>().result; \
//       trx.clear(); \
//    }
// #define CustomAuditActor(actor_name)                                \
//    if( actor_name ## _id != account_id_type() )                     \
//    {                                                                \
//       CHECK_BALANCE( actor_name, a ## actor_name.bal );             \
//       CHECK_CASHBACK_VBO( actor_name, a ## actor_name.ubal );       \
//    }

// #define CustomAudit()                                \
//    {                                                 \
//       CustomAuditActor( life );                      \
//       CustomAuditActor( rog );                       \
//       CustomAuditActor( ann );                       \
//       CustomAuditActor( stud );                      \
//       CustomAuditActor( dumy );                      \
//       CustomAuditActor( scud );                      \
//       CustomAuditActor( pleb );                      \
//    }

//    int64_t reg_fee    = fees->get< account_create_operation >().premium_fee;
//    int64_t xfer_fee   = fees->get< transfer_operation >().fee;
//    int64_t upg_an_fee = fees->get< account_upgrade_operation >().membership_annual_fee;
//    int64_t upg_lt_fee = fees->get< account_upgrade_operation >().membership_lifetime_fee;
//    // all percentages here are cut from whole pie!
//    uint64_t network_pct = 20 * P1;
//    uint64_t lt_pct = 375 * P100 / 1000;

// //    BOOST_TEST_MESSAGE("Register and upgrade Ann");
// //    {
// //       CustomRegisterActor(ann, life, life, 75);
// //       alife.vcb += reg_fee; alife.bal += -reg_fee;
// //       CustomAudit();

// //       transfer(life_id, ann_id, asset(aann.b0));
// //       alife.vcb += xfer_fee; alife.bal += -xfer_fee -aann.b0; aann.bal += aann.b0;
// //       CustomAudit();

// //       upgrade_to_annual_member(ann_id);
// //       aann.ucb += upg_an_fee; aann.bal += -upg_an_fee;

// //       // audit distribution of fees from Ann
// //       alife.ubal += pct( P100-network_pct, aann.ucb );
// //       alife.bal  += pct( P100-network_pct, aann.vcb );
// //       aann.ucb = 0; aann.vcb = 0;
// //       CustomAudit();
// //    }

//    BOOST_TEST_MESSAGE("Register dumy and stud");
//    CustomRegisterActor(dumy, rog, life, 75);
//    arog.vcb += reg_fee; arog.bal += -reg_fee;
//    CustomAudit();

//    CustomRegisterActor(stud, rog, ann, 80);
//    arog.vcb += reg_fee; arog.bal += -reg_fee;
//    CustomAudit();

//    BOOST_TEST_MESSAGE("Upgrade stud to lifetime member");

//    transfer(life_id, stud_id, asset(astud.b0));
//    alife.vcb += xfer_fee; alife.bal += -astud.b0 -xfer_fee; astud.bal += astud.b0;
//    CustomAudit();

//    upgrade_to_lifetime_member(stud_id);
//    astud.ucb += upg_lt_fee; astud.bal -= upg_lt_fee;

// /*
// network_cut:   20000
// referrer_cut:  40000 -> ann
// registrar_cut: 10000 -> rog
// lifetime_cut:  30000 -> life

// NET : net
// LTM : net' ltm
// REF : net' ltm' ref
// REG : net' ltm' ref'
// */

//    // audit distribution of fees from stud
//    alife.ubal += pct( P100-network_pct,      lt_pct,                     astud.ucb );
//    aann.ubal  += pct( P100-network_pct, P100-lt_pct,      astud.ref_pct, astud.ucb );
//    arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-astud.ref_pct, astud.ucb );
//    astud.ucb  = 0;
//    CustomAudit();

//    BOOST_TEST_MESSAGE("Register pleb and scud");

//    CustomRegisterActor(pleb, rog, stud, 95);
//    arog.vcb += reg_fee; arog.bal += -reg_fee;
//    CustomAudit();

//    CustomRegisterActor(scud, stud, ann, 80);
//    astud.vcb += reg_fee; astud.bal += -reg_fee;
//    CustomAudit();

//    generate_block();

//    BOOST_TEST_MESSAGE("Wait for maintenance interval");

//    generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
//    // audit distribution of fees from life
//    alife.ubal += pct( P100-network_pct, alife.ucb +alife.vcb );
//    alife.ucb = 0; alife.vcb = 0;

//    // audit distribution of fees from rog
//    arog.ubal += pct( P100-network_pct, arog.ucb + arog.vcb );
//    arog.ucb = 0; arog.vcb = 0;

//    // audit distribution of fees from ann
//    alife.ubal += pct( P100-network_pct,      lt_pct,                    aann.ucb+aann.vcb );
//    aann.ubal  += pct( P100-network_pct, P100-lt_pct,      aann.ref_pct, aann.ucb+aann.vcb );
//    alife.ubal += pct( P100-network_pct, P100-lt_pct, P100-aann.ref_pct, aann.ucb+aann.vcb );
//    aann.ucb = 0; aann.vcb = 0;

//    // audit distribution of fees from stud
//    astud.ubal += pct( P100-network_pct,                                  astud.ucb+astud.vcb );
//    astud.ucb = 0; astud.vcb = 0;

//    // audit distribution of fees from dumy
//    alife.ubal += pct( P100-network_pct,      lt_pct,                     adumy.ucb+adumy.vcb );
//    alife.ubal += pct( P100-network_pct, P100-lt_pct,      adumy.ref_pct, adumy.ucb+adumy.vcb );
//    arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-adumy.ref_pct, adumy.ucb+adumy.vcb );
//    adumy.ucb = 0; adumy.vcb = 0;

//    // audit distribution of fees from scud
//    alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
//    aann.ubal  += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
//    astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
//    ascud.ucb = 0; ascud.vcb = 0;

//    // audit distribution of fees from pleb
//    astud.ubal += pct( P100-network_pct,      lt_pct,                     apleb.ucb+apleb.vcb );
//    astud.ubal += pct( P100-network_pct, P100-lt_pct,      apleb.ref_pct, apleb.ucb+apleb.vcb );
//    arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-apleb.ref_pct, apleb.ucb+apleb.vcb );
//    apleb.ucb = 0; apleb.vcb = 0;

//    CustomAudit();

//    BOOST_TEST_MESSAGE("Doing some transfers");

//    transfer(stud_id, scud_id, asset(500000));
//    astud.bal += -500000-xfer_fee; astud.vcb += xfer_fee; ascud.bal += 500000;
//    CustomAudit();

//    transfer(scud_id, pleb_id, asset(400000));
//    ascud.bal += -400000-xfer_fee; ascud.vcb += xfer_fee; apleb.bal += 400000;
//    CustomAudit();

//    transfer(pleb_id, dumy_id, asset(300000));
//    apleb.bal += -300000-xfer_fee; apleb.vcb += xfer_fee; adumy.bal += 300000;
//    CustomAudit();

//    transfer(dumy_id, rog_id, asset(200000));
//    adumy.bal += -200000-xfer_fee; adumy.vcb += xfer_fee; arog.bal += 200000;
//    CustomAudit();

//    BOOST_TEST_MESSAGE("Waiting for maintenance time");

//    generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

//    // audit distribution of fees from life
//    alife.ubal += pct( P100-network_pct, alife.ucb +alife.vcb );
//    alife.ucb = 0; alife.vcb = 0;

//    // audit distribution of fees from rog
//    arog.ubal += pct( P100-network_pct, arog.ucb + arog.vcb );
//    arog.ucb = 0; arog.vcb = 0;

//    // audit distribution of fees from ann
//    alife.ubal += pct( P100-network_pct,      lt_pct,                    aann.ucb+aann.vcb );
//    aann.ubal  += pct( P100-network_pct, P100-lt_pct,      aann.ref_pct, aann.ucb+aann.vcb );
//    alife.ubal += pct( P100-network_pct, P100-lt_pct, P100-aann.ref_pct, aann.ucb+aann.vcb );
//    aann.ucb = 0; aann.vcb = 0;

//    // audit distribution of fees from stud
//    astud.ubal += pct( P100-network_pct,                                  astud.ucb+astud.vcb );
//    astud.ucb = 0; astud.vcb = 0;

//    // audit distribution of fees from dumy
//    alife.ubal += pct( P100-network_pct,      lt_pct,                     adumy.ucb+adumy.vcb );
//    alife.ubal += pct( P100-network_pct, P100-lt_pct,      adumy.ref_pct, adumy.ucb+adumy.vcb );
//    arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-adumy.ref_pct, adumy.ucb+adumy.vcb );
//    adumy.ucb = 0; adumy.vcb = 0;

//    // audit distribution of fees from scud
//    alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
//    aann.ubal  += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
//    astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
//    ascud.ucb = 0; ascud.vcb = 0;

//    // audit distribution of fees from pleb
//    astud.ubal += pct( P100-network_pct,      lt_pct,                     apleb.ucb+apleb.vcb );
//    astud.ubal += pct( P100-network_pct, P100-lt_pct,      apleb.ref_pct, apleb.ucb+apleb.vcb );
//    arog.ubal  += pct( P100-network_pct, P100-lt_pct, P100-apleb.ref_pct, apleb.ucb+apleb.vcb );
//    apleb.ucb = 0; apleb.vcb = 0;

//    CustomAudit();

//    BOOST_TEST_MESSAGE("Waiting for annual membership to expire");

//    generate_blocks(ann_id(*db).membership_expiration_date);
//    generate_block();

//    BOOST_TEST_MESSAGE("Transferring from scud to pleb");

//    //ann's membership has expired, so scud's fee should go up to life instead.
//    transfer(scud_id, pleb_id, asset(10));
//    ascud.bal += -10-xfer_fee; ascud.vcb += xfer_fee; apleb.bal += 10;
//    CustomAudit();

//    BOOST_TEST_MESSAGE("Waiting for maint interval");

//    generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

//    // audit distribution of fees from scud
//    alife.ubal += pct( P100-network_pct,      lt_pct,                     ascud.ucb+ascud.vcb );
//    alife.ubal += pct( P100-network_pct, P100-lt_pct,      ascud.ref_pct, ascud.ucb+ascud.vcb );
//    astud.ubal += pct( P100-network_pct, P100-lt_pct, P100-ascud.ref_pct, ascud.ucb+ascud.vcb );
//    ascud.ucb = 0; ascud.vcb = 0;

//    CustomAudit();

// } FC_LOG_AND_RETHROW() }

// BOOST_AUTO_TEST_CASE( account_create_fee_scaling )
// { try {
//    auto accounts_per_scale = db->get_global_properties().parameters.accounts_per_fee_scale;
//    db->modify(global_property_id_type()(*db), [](global_property_object& gpo)
//    {
//       gpo.parameters.current_fees = fee_schedule::get_default();
//       gpo.parameters.current_fees->get<account_create_operation>().basic_fee = 1;
//    });

//    for( int i = db->get_dynamic_global_properties().accounts_registered_this_interval; i < accounts_per_scale; ++i )
//    {
//       BOOST_CHECK_EQUAL(db->get_global_properties().parameters.current_fees->get<account_create_operation>().basic_fee, 1);
//       create_account("shill" + fc::to_string(i));
//    }
//    for( int i = 0; i < accounts_per_scale; ++i )
//    {
//       BOOST_CHECK_EQUAL(db->get_global_properties().parameters.current_fees->get<account_create_operation>().basic_fee, 16);
//       create_account("moreshills" + fc::to_string(i));
//    }
//    for( int i = 0; i < accounts_per_scale; ++i )
//    {
//       BOOST_CHECK_EQUAL(db->get_global_properties().parameters.current_fees->get<account_create_operation>().basic_fee, 256);
//       create_account("moarshills" + fc::to_string(i));
//    }
//    BOOST_CHECK_EQUAL(db->get_global_properties().parameters.current_fees->get<account_create_operation>().basic_fee, 4096);

//    generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);
//    BOOST_CHECK_EQUAL(db->get_global_properties().parameters.current_fees->get<account_create_operation>().basic_fee, 1);
// } FC_LOG_AND_RETHROW() }


// BOOST_AUTO_TEST_CASE( fee_refund_test )
// {
//    try {
//       ACTORS((antelopes)(zebres)(mustang));

//       int64_t antelopes_b0 = 1000000, zebres_b0 = 1000000;

//       transfer( account_id_type(), antelopes_id, asset(antelopes_b0) );
//       transfer( account_id_type(), zebres_id, asset(zebres_b0) );

//       asset_id_type core_id = asset_id_type();
//       asset_id_type usd_id = create_user_issued_asset( "MUSTANGUSD", mustang_id(*db), charge_market_fee ).id;
//       issue_uia( antelopes_id, asset( antelopes_b0, usd_id ) );
//       issue_uia( zebres_id, asset( zebres_b0, usd_id ) );

//       int64_t order_create_fee = 537;
//       int64_t order_cancel_fee = 129;

//       uint32_t skip = database::skip_witness_signature
//                     | database::skip_transaction_signatures
//                     | database::skip_transaction_dupe_check
//                     | database::skip_block_size_check
//                     | database::skip_tapos_check
//                     | database::skip_authority_check
//                     | database::skip_merkle_check
//                     ;

//       generate_block( skip );

//       for( int i=0; i<2; i++ )
//       {
//          /*
//          if( i == 1 )
//          {
//             generate_blocks( HARDFORK_445_TIME, true, skip );
//             generate_block( skip );
//          }
//          */

//          // enable_fees() and change_fees() modifies DB directly, and results will be overwritten by block generation
//          // so we have to do it every time we stop generating/popping blocks and start doing tx's
//          enable_fees();
//          /*
//          change_fees({
//                        limit_order_create_operation::fee_parameters_type { order_create_fee },
//                        limit_order_cancel_operation::fee_parameters_type { order_cancel_fee }
//                      });
//          */
//          // C++ -- The above commented out statement doesn't work, I don't know why
//          // so we will use the following rather lengthy initialization instead
//          {
//             flat_set< fee_parameters > new_fees;
//             {
//                limit_order_create_operation::fee_parameters_type create_fee_params;
//                create_fee_params.fee = order_create_fee;
//                new_fees.insert( create_fee_params );
//             }
//             {
//                limit_order_cancel_operation::fee_parameters_type cancel_fee_params;
//                cancel_fee_params.fee = order_cancel_fee;
//                new_fees.insert( cancel_fee_params );
//             }
//             change_fees( new_fees );
//          }

//          // antelopes creates order
//          // zebres creates order which doesn't match

//          // AAAAGGHH create_sell_order reads trx.expiration #469
//          set_expiration( db.get(), trx );

//          // Check non-overlapping

//          limit_order_id_type ao1_id = create_sell_order( antelopes_id, asset(1000), asset(1000, usd_id) )->id;
//          limit_order_id_type bo1_id = create_sell_order(   zebres_id, asset(500, usd_id), asset(1000) )->id;

//          BOOST_CHECK_EQUAL( get_balance( antelopes_id, core_id ), antelopes_b0 - 1000 - order_create_fee );
//          BOOST_CHECK_EQUAL( get_balance( antelopes_id,  usd_id ), antelopes_b0 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id, core_id ), zebres_b0 - order_create_fee );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id,  usd_id ), zebres_b0 - 500 );

//          // zebres cancels order
//          cancel_limit_order( bo1_id(*db) );

//          int64_t cancel_net_fee;

//          cancel_net_fee = order_cancel_fee;

//          BOOST_CHECK_EQUAL( get_balance( antelopes_id, core_id ), antelopes_b0 - 1000 - order_create_fee );
//          BOOST_CHECK_EQUAL( get_balance( antelopes_id,  usd_id ), antelopes_b0 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id, core_id ), zebres_b0 - cancel_net_fee );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id,  usd_id ), zebres_b0 );

//          // antelopes cancels order
//          cancel_limit_order( ao1_id(*db) );

//          BOOST_CHECK_EQUAL( get_balance( antelopes_id, core_id ), antelopes_b0 - cancel_net_fee );
//          BOOST_CHECK_EQUAL( get_balance( antelopes_id,  usd_id ), antelopes_b0 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id, core_id ), zebres_b0 - cancel_net_fee );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id,  usd_id ), zebres_b0 );

//          // Check partial fill
//          const limit_order_object* ao2 = create_sell_order( antelopes_id, asset(1000), asset(200, usd_id) );
//          const limit_order_object* bo2 = create_sell_order(   zebres_id, asset(100, usd_id), asset(500) );

//          BOOST_CHECK( ao2 != nullptr );
//          BOOST_CHECK( bo2 == nullptr );

//          BOOST_CHECK_EQUAL( get_balance( antelopes_id, core_id ), antelopes_b0 - cancel_net_fee - order_create_fee - 1000 );
//          BOOST_CHECK_EQUAL( get_balance( antelopes_id,  usd_id ), antelopes_b0 + 100 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id, core_id ),   zebres_b0 - cancel_net_fee - order_create_fee + 500 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id,  usd_id ),   zebres_b0 - 100 );

//          // cancel antelopes order, show that entire deferred_fee was consumed by partial match
//          cancel_limit_order( *ao2 );

//          BOOST_CHECK_EQUAL( get_balance( antelopes_id, core_id ), antelopes_b0 - cancel_net_fee - order_create_fee - 500 - order_cancel_fee );
//          BOOST_CHECK_EQUAL( get_balance( antelopes_id,  usd_id ), antelopes_b0 + 100 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id, core_id ),   zebres_b0 - cancel_net_fee - order_create_fee + 500 );
//          BOOST_CHECK_EQUAL( get_balance(   zebres_id,  usd_id ),   zebres_b0 - 100 );

//          // TODO: Check multiple fill
//          // there really should be a test case involving antelopes creating multiple orders matched by single zebres order
//          // but we'll save that for future cleanup

//          // undo above tx's and reset
//          generate_block( skip );
//          db->pop_block();
//       }
//    }
//    FC_LOG_AND_RETHROW()
// }

BOOST_AUTO_TEST_CASE( stealth_fba_test )
{
   try
   {
      ACTORS( (antelopes)(zebras)(chloes)(bingss)(dingss)(philbin)(tomcat) );
      upgrade_to_lifetime_member(philbin_id);
      generate_blocks( fc::time_point_sec(1459789200) );

      // Philbin (registrar who registers Rex)

      // dingss (initial issuer of stealth asset, will later transfer to tomcat)
      // antelopes, zebras, chloes, bingss (ABCD)
      // Rex (recycler -- buyback account for stealth asset)
      // tomcat (owner of stealth asset who will be set as top_n authority)

      // dingss creates STEALTH
      asset_id_type stealth_id = create_user_issued_asset( "STEALTH", dingss_id(*db),
        transfer_restricted | override_authority | white_list | charge_market_fee ).id;

      /*
      // this is disabled because it doesn't work, our modify() is probably being overwritten by undo

      //
      // Init blockchain with stealth ID's
      // On a real chain, this would be done with #define GRAPHENE_FBA_STEALTH_DESIGNATED_ASSET
      // causing the designated_asset fields of these objects to be set at genesis, but for
      // this test we modify the db directly.
      //
      auto set_fba_asset = [&]( uint64_t fba_acc_id, asset_id_type asset_id )
      {
         db.modify( fba_accumulator_id_type(fba_acc_id)(*db), [&]( fba_accumulator_object& fba )
         {
            fba.designated_asset = asset_id;
         } );
      };

      set_fba_asset( fba_accumulator_id_transfer_to_blind  , stealth_id );
      set_fba_asset( fba_accumulator_id_blind_transfer     , stealth_id );
      set_fba_asset( fba_accumulator_id_transfer_from_blind, stealth_id );
      */

      // dingss kills some permission bits (this somehow happened to the real STEALTH in production)
      {
         asset_update_operation update_op;
         update_op.issuer = dingss_id;
         update_op.asset_to_update = stealth_id;
         asset_options new_options;
         new_options = stealth_id(*db).options;
         new_options.issuer_permissions = charge_market_fee;
         new_options.flags = transfer_restricted | override_authority | white_list | charge_market_fee;
         // after fixing #579 you should be able to delete the following line
         new_options.core_exchange_rate = price( asset( 1, stealth_id ), asset( 1, asset_id_type() ) );
         update_op.new_options = new_options;
         signed_transaction tx;
         tx.operations.push_back( update_op );
         set_expiration( db.get(), tx );
         sign( tx, dingss_private_key );
         PUSH_TX( db.get(), tx );
      }

      // dingss transfers issuer duty to tomcat
      {
         asset_update_operation update_op;
         update_op.issuer = dingss_id;
         update_op.asset_to_update = stealth_id;
         update_op.new_issuer = tomcat_id;
         // new_options should be optional, but isn't...the following line should be unnecessary #580
         update_op.new_options = stealth_id(*db).options;
         signed_transaction tx;
         tx.operations.push_back( update_op );
         set_expiration( db.get(), tx );
         sign( tx, dingss_private_key );
         PUSH_TX( db.get(), tx );
      }

      // tomcat re-enables the permission bits to clear the flags, then clears them again
      // Allowed by #572 when current_supply == 0
      {
         asset_update_operation update_op;
         update_op.issuer = tomcat_id;
         update_op.asset_to_update = stealth_id;
         asset_options new_options;
         new_options = stealth_id(*db).options;
         new_options.issuer_permissions = new_options.flags | charge_market_fee;
         update_op.new_options = new_options;
         signed_transaction tx;
         // enable perms is one op
         tx.operations.push_back( update_op );

         new_options.issuer_permissions = charge_market_fee;
         new_options.flags = charge_market_fee;
         update_op.new_options = new_options;
         // reset wrongly set flags and reset permissions can be done in a single op
         tx.operations.push_back( update_op );

         set_expiration( db.get(), tx );
         sign( tx, tomcat_private_key );
         PUSH_TX( db.get(), tx );
      }

      // Philbin registers Rex who will be the asset's buyback, includingss sig from the new issuer (tomcat)
    //   account_id_type rex_id;
    //   {
    //      buyback_account_options bbo;
    //      bbo.asset_to_buy = stealth_id;
    //      bbo.asset_to_buy_issuer = tomcat_id;
    //      bbo.markets.emplace( asset_id_type() );
    //      account_create_operation create_op = make_account( "rex" );
    //      create_op.registrar = philbin_id;
    //      create_op.extensions.value.buyback_options = bbo;
    //      create_op.owner = authority::null_authority();
    //      create_op.active = authority::null_authority();

    //      signed_transaction tx;
    //      tx.operations.push_back( create_op );
    //      set_expiration( db.get(), tx );
    //      sign( tx, philbin_private_key );
    //      sign( tx, tomcat_private_key );

    //      processed_transaction ptx = PUSH_TX( db.get(), tx );
    //      rex_id = ptx.operation_results.back().get< object_id_result>().result;
    //   }

      // tomcat issues some asset to antelopes and zebras
      set_expiration( db.get(), trx );  // #11
      issue_uia( antelopes_id, asset( 1000, stealth_id ) );
      issue_uia(   zebras_id, asset( 1000, stealth_id ) );

      // tomcat sets his authority to the top_n of the asset
      {
         top_holders_special_authority top2;
         top2.num_top_holders = 2;
         top2.asset = stealth_id;

         account_update_operation op;
         op.account = tomcat_id;
         op.extensions.value.active_special_authority = top2;
         op.extensions.value.owner_special_authority = top2;

         signed_transaction tx;
         tx.operations.push_back( op );

         set_expiration( db.get(), tx );
         sign( tx, tomcat_private_key );

         PUSH_TX( db.get(), tx );
      }

      // Wait until the next maintenance interval for top_n to take effect
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      // Do a blind op to add some fees to the pool.
      fund( chloes_id(*db), asset( 100000, asset_id_type() ) );

      // auto create_transfer_to_blind = [&]( account_id_type account, asset amount, const std::string& key ) -> transfer_to_blind_operation
      // {
      //    fc::ecc::private_key blind_key = fc::ecc::private_key::regenerate( fc::sha256::hash( key+"-privkey" ) );
      //    public_key_type blind_pub = blind_key.get_public_key();

      //    fc::sha256 secret = fc::sha256::hash( key+"-secret" );
      //    fc::sha256 nonce = fc::sha256::hash( key+"-nonce" );

      //    transfer_to_blind_operation op;
      //    blind_output blind_out;
      //    blind_out.owner = authority( 1, blind_pub, 1 );
      //    blind_out.commitment = fc::ecc::blind( secret, amount.amount.value );
      //    blind_out.range_proof = fc::ecc::range_proof_sign( 0, blind_out.commitment, secret, nonce, 0, 0, amount.amount.value );

      //    op.amount = amount;
      //    op.from = account;
      //    op.blindingss_factor = fc::ecc::blind_sum( {secret}, 1 );
      //    op.outputs = {blind_out};

      //    return op;
      // };

      // {
      //    transfer_to_blind_operation op = create_transfer_to_blind( chloes_id, asset( 5000, asset_id_type() ), "chloes-key" );
      //    // op.fee = asset( 1000, asset_id_type() );

      //    signed_transaction tx;
      //    tx.operations.push_back( op );
      //    set_expiration( db.get(), tx );
      //    sign( tx, chloes_private_key );

      //    PUSH_TX( db, tx );
      // }

      // wait until next maint interval
      generate_blocks(db->get_dynamic_global_properties().next_maintenance_time);

      idump( ( get_operation_history( chloes_id ) ) );
    //   idump( ( get_operation_history( rex_id ) ) );
      idump( ( get_operation_history( tomcat_id ) ) );
   }
   catch( const fc::exception& e )
   {
      elog( "caught exception ${e}", ("e", e.to_detail_string()) );
      throw;
   }
}

BOOST_AUTO_TEST_CASE( defaults_test )
{ try {
    fee_schedule schedule;
    const limit_order_create_operation::fee_parameters_type default_order_fee;

    // no fees set yet -> default
    asset fee = schedule.calculate_fee( limit_order_create_operation() );
    BOOST_CHECK_EQUAL( default_order_fee.fee, fee.amount.value );

    limit_order_create_operation::fee_parameters_type new_order_fee; new_order_fee.fee = 123;
    // set fee + check
    schedule.parameters.insert( new_order_fee );
    fee = schedule.calculate_fee( limit_order_create_operation() );
    BOOST_CHECK_EQUAL( new_order_fee.fee, fee.amount.value );

    // bid_collateral fee defaults to call_order_update fee
    // call_order_update fee is unset -> default
    const call_order_update_operation::fee_parameters_type default_short_fee;
    call_order_update_operation::fee_parameters_type new_short_fee; new_short_fee.fee = 123;
    fee = schedule.calculate_fee( bid_collateral_operation() );
    BOOST_CHECK_EQUAL( default_short_fee.fee, fee.amount.value );

    // set call_order_update fee + check bid_collateral fee
    schedule.parameters.insert( new_short_fee );
    fee = schedule.calculate_fee( bid_collateral_operation() );
    BOOST_CHECK_EQUAL( new_short_fee.fee, fee.amount.value );

    // set bid_collateral fee + check
    bid_collateral_operation::fee_parameters_type new_bid_fee; new_bid_fee.fee = 124;
    schedule.parameters.insert( new_bid_fee );
    fee = schedule.calculate_fee( bid_collateral_operation() );
    BOOST_CHECK_EQUAL( new_bid_fee.fee, fee.amount.value );
  }
  catch( const fc::exception& e )
  {
     elog( "caught exception ${e}", ("e", e.to_detail_string()) );
     throw;
  }
}
/*
BOOST_AUTO_TEST_CASE( issue_429_test )
{
   try
   {
      ACTORS((antelope));

      transfer( committee_account, antelope_id, asset( 1000000 * asset::scaled_precision( asset_id_type()(*db).precision ) ) );

      // make sure the database requires our fee to be nonzero
      enable_fees();

      auto fees_to_pay = db.get_global_properties().parameters.current_fees->get<asset_create_operation>();

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = antelope_id;
         op.symbol = "antelope";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset( (fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) & (~1) );
         tx.operations.push_back( op );
         set_expiration( db.get(), tx );
         sign( tx, antelope_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = antelope_id;
         op.symbol = "antelope.ODD";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset((fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) | 1);
         tx.operations.push_back( op );
         set_expiration( db.get(), tx );
         sign( tx, antelope_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );

      //generate_blocks( HARDFORK_CORE_429_TIME + 10 );

      {
         signed_transaction tx;
         asset_create_operation op;
         op.issuer = antelope_id;
         op.symbol = "antelope.ODDER";
         op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
         op.fee = asset((fees_to_pay.long_symbol + fees_to_pay.price_per_kbyte) | 1);
         tx.operations.push_back( op );
         set_expiration( db.get(), tx );
         sign( tx, antelope_private_key );
         PUSH_TX( db, tx );
      }

      verify_asset_supplies( db );
   }
   catch( const fc::exception& e )
   {
      edump((e.to_detail_string()));
      throw;
   }
}
*/
// BOOST_AUTO_TEST_CASE( issue_433_test )
// {
//    try
//    {
//       ACTORS((antelope));

//       auto& core = asset_id_type()(*db);

//       transfer( committee_account, antelope_id, asset( 1000000 * asset::scaled_precision( core.precision ) ) );

//       const auto& myusd = create_user_issued_asset( "MYUSD", antelope, 0 );
//       issue_uia( antelope, myusd.amount( 2000000000 ) );

//       // make sure the database requires our fee to be nonzero
//       enable_fees();

//       const auto& fees = *db->get_global_properties().parameters.current_fees;
//       const auto asset_create_fees = fees.get<asset_create_operation>();

//     //   fund_fee_pool( antelope, myusd, 5*asset_create_fees.long_symbol );

//       asset_create_operation op;
//       op.issuer = antelope_id;
//       op.symbol = "antelope";
//       op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
//       // op.fee = myusd.amount( ((asset_create_fees.long_symbol + asset_create_fees.price_per_kbyte) & (~1)) );
//       signed_transaction tx;
//       tx.operations.push_back( op );
//       set_expiration( db.get(), tx );
//       sign( tx, antelope_private_key );
//       PUSH_TX( db.get(), tx );

//       verify_asset_supplies( db.get() );
//    }
//    catch( const fc::exception& e )
//    {
//       edump((e.to_detail_string()));
//       throw;
//    }
// }

// BOOST_AUTO_TEST_CASE( issue_433_indirect_test )
// {
//    try
//    {
//       ACTORS((antelope));

//       auto& core = asset_id_type()(*db);

//       transfer( committee_account, antelope_id, asset( 1000000 * asset::scaled_precision( core.precision ) ) );

//       const auto& myusd = create_user_issued_asset( "MYUSD", antelope, 0 );
//       issue_uia( antelope, myusd.amount( 2000000000 ) );

//       // make sure the database requires our fee to be nonzero
//       enable_fees();

//       const auto& fees = *db->get_global_properties().parameters.current_fees;
//       const auto asset_create_fees = fees.get<asset_create_operation>();

//     //   fund_fee_pool( antelope, myusd, 5*asset_create_fees.long_symbol );

//       asset_create_operation op;
//       op.issuer = antelope_id;
//       op.symbol = "antelope";
//       op.common_options.core_exchange_rate = asset( 1 ) / asset( 1, asset_id_type( 1 ) );
//       // op.fee = myusd.amount( ((asset_create_fees.long_symbol + asset_create_fees.price_per_kbyte) & (~1)) );

//       const auto proposal_create_fees = fees.get<proposal_create_operation>();
//       proposal_create_operation prop;
//       prop.fee_paying_account = antelope_id;
//       prop.proposed_ops.emplace_back( op );
//       prop.expiration_time =  db->head_block_time() + fc::days(1);
//       // prop.fee = asset( proposal_create_fees.fee + proposal_create_fees.price_per_kbyte );
//       object_id_type proposal_id;
//       {
//          signed_transaction tx;
//          tx.operations.push_back( prop );
//          set_expiration( db.get(), tx );
//          sign( tx, antelope_private_key );
//          proposal_id = PUSH_TX( db.get(), tx ).operation_results.front().get<object_id_result>().result;
//       }
//       const proposal_object& proposal = db->get<proposal_object>( proposal_id );

//       const auto proposal_update_fees = fees.get<proposal_update_operation>();
//       proposal_update_operation pup;
//       pup.proposal = proposal.id;
//       pup.fee_paying_account = antelope_id;
//       pup.active_approvals_to_add.insert(antelope_id);
//     //   pup.fee = asset( proposal_update_fees.fee + proposal_update_fees.price_per_kbyte );
//       {
//          signed_transaction tx;
//          tx.operations.push_back( pup );
//          set_expiration( db.get(), tx );
//          sign( tx, antelope_private_key );
//          PUSH_TX( db.get(), tx );
//       }

//       verify_asset_supplies( db );
//    }
//    catch( const fc::exception& e )
//    {
//       edump((e.to_detail_string()));
//       throw;
//    }
// }

BOOST_AUTO_TEST_SUITE_END()
