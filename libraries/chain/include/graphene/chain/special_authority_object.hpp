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
 * special_authority_object only exists to help with a specific indexing problem.
 * We want to be able to iterate over all accounts that contain a special authority.
 * However, accounts which have a special_authority are very rare.  So rather
 * than indexing account_object by the special_authority fields (requiring additional
 * bookkeeping for every account), we instead maintain a special_authority_object
 * pointing to each account which has special_authority (requiring additional
 * bookkeeping only for every account which has special_authority).
 *
 * This class is an implementation detail.
 */

class special_authority_object : public graphene::db::abstract_object<special_authority_object>
{
   public:
      static const uint8_t space_id = implementation_ids;
      static const uint8_t type_id = impl_special_authority_object_type;

      account_id_type account;
};

struct by_account;

typedef multi_index_container<
   special_authority_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      ordered_unique< tag<by_account>, member< special_authority_object, account_id_type, &special_authority_object::account> >
   >
> special_authority_multi_index_type;

typedef generic_index< special_authority_object, special_authority_multi_index_type > special_authority_index;

} } // graphene::chain

FC_REFLECT_DERIVED(
   graphene::chain::special_authority_object,
   (graphene::db::object),
   (account)
)
