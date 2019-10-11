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

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/hardfork.hpp>

namespace graphene
{
namespace chain
{

namespace detail
{

bool _is_authorized_asset(
    const database &d,
    const account_object &acct,
    const asset_object &asset_obj)
{
   const auto &index = d.get_index_type<asset_restricted_index>().indices().get<by_asset_and_restricted_enum>();
   if (!asset_obj.is_transfer_restricted())
   {
      if (index.find(boost::make_tuple(asset_obj.id, restricted_enum::blacklist_authorities, acct.id)) != index.end())
      {
         return false;
      }
   }
   const auto &range = index.equal_range(boost::make_tuple(asset_obj.id, restricted_enum::whitelist_authorities));
   if (!boost::distance(range))
      return true;
   if (index.find(boost::make_tuple(asset_obj.id, restricted_enum::whitelist_authorities, acct.id)) != index.end())
      return true;

   return false;
}

} // namespace detail

} // namespace chain
} // namespace graphene
