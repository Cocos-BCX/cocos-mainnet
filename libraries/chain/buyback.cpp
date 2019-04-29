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

#include <graphene/chain/protocol/buyback.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/exceptions.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene { namespace chain {

void evaluate_buyback_account_options( const database& db, const buyback_account_options& bbo )
{
   const asset_object& a = bbo.asset_to_buy(db);
   GRAPHENE_ASSERT( a.issuer == bbo.asset_to_buy_issuer,
      account_create_buyback_incorrect_issuer, "Incorrect asset issuer specified in buyback_account_options", ("asset", a)("bbo", bbo) );
   GRAPHENE_ASSERT( !a.buyback_account.valid(),
      account_create_buyback_already_exists, "Cannot create buyback for asset which already has buyback", ("asset", a)("bbo", bbo) );
   // TODO:  Replace with chain parameter #554
   GRAPHENE_ASSERT( bbo.markets.size() < GRAPHENE_DEFAULT_MAX_BUYBACK_MARKETS,
      account_create_buyback_too_many_markets, "Too many buyback markets", ("asset", a)("bbo", bbo) );
}

} }
