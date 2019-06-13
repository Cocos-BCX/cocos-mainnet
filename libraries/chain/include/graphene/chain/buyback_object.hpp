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

#include <graphene/chain/protocol/types.hpp>
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

/**
 * buyback_authority_object only exists to help with a specific indexing problem.
 * We want to be able to iterate over all assets that have a buyback program.
 * However, assets which have a buyback program are very rare.  So rather
 * than indexing asset_object by the buyback field (requiring additional
 * bookkeeping for every asset), we instead maintain a buyback_object
 * pointing to each asset which has buyback (requiring additional
 * bookkeeping only for every asset which has buyback).
 *
 * This class is an implementation detail.
 */

class buyback_object : public graphene::db::abstract_object< buyback_object >
{
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_buyback_object_type;

      asset_id_type asset_to_buy;
};

struct by_asset;

typedef multi_index_container<
   buyback_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_asset>, member< buyback_object, asset_id_type, &buyback_object::asset_to_buy> >
   >
> buyback_multi_index_type;

typedef generic_index< buyback_object, buyback_multi_index_type > buyback_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::buyback_object, (graphene::db::object), (asset_to_buy) )
