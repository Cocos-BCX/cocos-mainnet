/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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

#include <graphene/app/database_api.hpp>
#include <graphene/wallet/wallet.hpp>

#include <iostream>

#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;
using namespace graphene::wallet;

BOOST_FIXTURE_TEST_SUITE(wallet_tests, database_fixture)

  /***
   * Check the basic behavior of deriving potential owner keys from a brain key
   */
  BOOST_AUTO_TEST_CASE(derive_owner_keys_from_brain_key) {
      try {
          /***
           * Act
           */
          int nbr_keys_desired = 3;
          vector<brain_key_info> derived_keys = graphene::wallet::utility::derive_owner_keys_from_brain_key("SOME WORDS GO HERE", nbr_keys_desired);


          /***
           * Assert: Check the number of derived keys
           */
          BOOST_CHECK_EQUAL(nbr_keys_desired, derived_keys.size());

          /***
           * Assert: Check that each derived key is unique
           */
          set<string> set_derived_public_keys;
          for (auto info : derived_keys) {
              string description = (string) info.pub_key;
              set_derived_public_keys.emplace(description);
          }
          BOOST_CHECK_EQUAL(nbr_keys_desired, set_derived_public_keys.size());

          /***
           * Assert: Check whether every public key begins with the expected prefix
           */
          string expected_prefix = GRAPHENE_ADDRESS_PREFIX;
          for (auto info : derived_keys) {
              string description = (string) info.pub_key;
              BOOST_CHECK_EQUAL(0, description.find(expected_prefix));
          }

      } FC_LOG_AND_RETHROW()
  }

BOOST_AUTO_TEST_SUITE_END()
