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

#include <graphene/chain/protocol/transaction.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {


/**
 *  @brief tracks the approval of a partially approved transaction 
 *  @ingroup object
 *  @ingroup protocol
 */
class proposal_object : public abstract_object<proposal_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id = proposal_object_type;

      time_point_sec                expiration_time;
      optional<time_point_sec>      review_period_time;
      transaction                   proposed_transaction;
      flat_set<account_id_type>     required_active_approvals;
      flat_set<account_id_type>     available_active_approvals;
      flat_set<account_id_type>     required_owner_approvals;
      flat_set<account_id_type>     available_owner_approvals;
      flat_set<public_key_type>     available_key_approvals;
      tx_hash_type                  trx_hash;
      bool                          allow_execution=false;

      bool is_authorized_to_execute(database& db)const;
};

/**
 *  @brief tracks all of the proposal objects that requrie approval of
 *  an individual account.   
 *
 *  @ingroup object
 *  @ingroup protocol
 *
 *  This is a secondary index on the proposal_index
 *
 *  @note the set of required approvals is constant
 */
class required_approval_index : public secondary_index
{
   public:
      virtual void object_inserted( const object& obj ) override;
      virtual void object_removed( const object& obj ) override;
      virtual void about_to_modify( const object& before ) override{};
      virtual void object_modified( const object& after  ) override{};

      void remove( account_id_type a, proposal_id_type p );

      map<account_id_type, set<proposal_id_type> > _account_to_proposals;
};

struct by_expiration{};
struct by_expiration_and_isruning{};
typedef boost::multi_index_container<
   proposal_object,
   indexed_by<
      ordered_unique< tag< by_id >, member< object, object_id_type, &object::id > >,
      ordered_non_unique< tag< by_expiration_and_isruning >,
                        composite_key<proposal_object,
                        member< proposal_object, bool, &proposal_object::allow_execution >,
                        member< proposal_object, time_point_sec, &proposal_object::expiration_time > 
                        >
             >
   >
> proposal_multi_index_container;
typedef generic_index<proposal_object, proposal_multi_index_container> proposal_index;

} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::proposal_object, (graphene::chain::object),
                    (expiration_time)(review_period_time)(proposed_transaction)(required_active_approvals)
                    (available_active_approvals)(required_owner_approvals)(available_owner_approvals)
                    (available_key_approvals)(trx_hash)(allow_execution) )
