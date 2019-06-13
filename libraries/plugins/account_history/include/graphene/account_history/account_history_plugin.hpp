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

#include <graphene/app/plugin.hpp>
#include <graphene/chain/database.hpp>

#include <graphene/chain/operation_history_object.hpp>

#include <fc/thread/future.hpp>

namespace graphene { namespace account_history {
   using namespace chain;
   //using namespace graphene::db;
   //using boost::multi_index_container;
   //using namespace boost::multi_index;

//
// Plugins should #define their SPACE_ID's so plugins with
// conflicting SPACE_ID assignments can be compiled into the
// same binary (by simply re-assigning some of the conflicting #defined
// SPACE_ID's in a build script).
//
// Assignment of SPACE_ID's cannot be done at run-time because
// various template automagic depends on them being known at compile
// time.
//
#ifndef ACCOUNT_HISTORY_SPACE_ID
#define ACCOUNT_HISTORY_SPACE_ID 4
#endif

enum account_history_object_type
{
   key_account_object_type = 0
};


namespace detail
{
    class account_history_plugin_impl;
}

class account_history_plugin : public graphene::app::plugin
{
   public:
      account_history_plugin();
      virtual ~account_history_plugin();

      std::string plugin_name()const override;
      virtual void plugin_set_program_options(
         boost::program_options::options_description& cli,
         boost::program_options::options_description& cfg) override;
      virtual void plugin_initialize(const boost::program_options::variables_map& options) override;
      virtual void plugin_startup() override;

      flat_set<account_id_type> tracked_accounts()const;

      friend class detail::account_history_plugin_impl;
      std::unique_ptr<detail::account_history_plugin_impl> my;
};

} } //graphene::account_history

/*struct by_id;
struct by_seq;
struct by_op;
typedef boost::multi_index_container<
   graphene::chain::account_transaction_history_object,
   boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      boost::multi_index::ordered_unique< tag<by_seq>,
         composite_key< account_transaction_history_object,
            member< account_transaction_history_object, account_id_type, &account_transaction_history_object::account>,
            member< account_transaction_history_object, uint32_t, &account_transaction_history_object::sequence>
         >
      >,
      boost::multi_index::ordered_unique< tag<by_op>,
         composite_key< account_transaction_history_object,
            member< account_transaction_history_object, account_id_type, &account_transaction_history_object::account>,
            member< account_transaction_history_object, operation_history_id_type, &account_transaction_history_object::operation_id>
         >
      >
   >
> account_transaction_history_multi_index_type;

typedef graphene::account_history::generic_index<graphene::chain::account_transaction_history_object, account_transaction_history_multi_index_type> account_transaction_history_index;
*/
