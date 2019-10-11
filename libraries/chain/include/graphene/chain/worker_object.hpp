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
#include <graphene/db/object.hpp>
#include <graphene/db/generic_index.hpp>

namespace graphene { namespace chain {

/**
  * @defgroup worker_types Implementations of the various worker types in the system
  *
  * The system has various worker types, which do different things with the money they are paid. These worker types
  * and their semantics are specified here.
  *
  * All worker types exist as a struct containing the data this worker needs to evaluate, as well as a method
  * pay_worker, which takes a pay amount and a non-const database reference, and applies the worker's specific pay
  * semantics to the worker_type struct and/or the database. Furthermore, all worker types have an initializer,
  * which is a struct containing the data needed to create that kind of worker.
  *
  * Each initializer type has a method, init, which takes a non-const database reference, a const reference to the
  * worker object being created, and a non-const reference to the specific *_worker_type object to initialize. The
  * init method creates any further objects, and initializes the worker_type object as necessary according to the
  * semantics of that particular worker type.
  *
  * To create a new worker type, define a my_new_worker_type struct with a pay_worker method which updates the
  * my_new_worker_type object and/or the database. Create a my_new_worker_type::initializer struct with an init
  * method and any data members necessary to create a new worker of this type. Reflect my_new_worker_type and
  * my_new_worker_type::initializer into FC's type system, and add them to @ref worker_type and @ref
  * worker_initializer respectively. Make sure the order of types in @ref worker_type and @ref worker_initializer
  * remains the same.
  * @{
  */
/**
 * @brief A worker who returns all of his pay to the reserve
 *
 * This worker type pays everything he receives back to the network's reserve funds pool.
 */
struct destroy_worker_type
{
   /// Record of how much this worker has burned in his lifetime
   share_type total_burned;

   void pay_worker(share_type pay, database&);
};

/**
 * @brief A worker who sends his pay to a vesting balance
 *
 * This worker type takes all of his pay and places it into a vesting balance
 */
struct vesting_balance_worker_type
{
   /// The balance this worker pays into
   vesting_balance_id_type balance;

   void pay_worker(share_type pay, database& db);
};

/**
 * @brief A worker who permanently destroys all of his pay
 *
 * This worker sends all pay he receives to the null account.
 */
struct burn_worker_type
{
   /// Record of how much this worker has burned in his lifetime
   share_type total_burned;

   void pay_worker(share_type pay, database&);
};

struct issuance_worker_type
{
   /// Record of how much this worker has burned in his lifetime
   share_type total_issuance=0;
   void pay_worker(share_type pay, database&db);
};



///@}

// The ordering of types in these two static variants MUST be the same.
typedef static_variant<
   destroy_worker_type,
   vesting_balance_worker_type,
   burn_worker_type,
   issuance_worker_type
> worker_type;


/**
 * @brief Worker object contains the details of a blockchain worker. See @ref workers for details.
 */
class worker_object : public abstract_object<worker_object>
{
   public:
      static const uint8_t space_id = protocol_ids;
      static const uint8_t type_id =  worker_object_type;

      /// ID of the account which owns this worker
      fc::optional<account_id_type> beneficiary;
      /// Time at which this worker begins receiving pay, if elected
      time_point_sec work_begin_date;
      /// Time at which this worker will cease to receive pay. Worker will be deleted at this time
      time_point_sec work_end_date;
      /// Amount in CORE this worker will be paid each day
      share_type daily_pay;
      /// ID of this worker's pay balance
      worker_type worker;
      /// Human-readable name for the worker
      string name;
      /// URL to a web page representing this worker
      string describe;

      bool completed=false;

      bool issuance_or_destroy()const
      {
         return worker.which()==worker_type::tag<issuance_worker_type>::value||worker.which()==worker_type::tag<destroy_worker_type>::value;
      }//nico log::是否是增发工作

      bool is_active(fc::time_point_sec now)const {
         return now >= work_begin_date && now <= work_end_date;
      }
};

struct by_account;
struct by_completed{};
struct by_vote_for;
struct by_vote_against;
typedef multi_index_container<
   worker_object,
   indexed_by<
      ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
      /*
      ordered_non_unique< tag<by_end_date_and_issuance>,
                        composite_key<worker_object,
                           const_mem_fun< worker_object, bool, &worker_object::issuance>,
                           member<worker_object, time_point_sec, &worker_object::work_end_date >>
                        >
      */
     ordered_non_unique<tag<by_completed>,member< worker_object, bool, &worker_object::completed>>
   >
> worker_object_multi_index_type;

using worker_index = generic_index<worker_object, worker_object_multi_index_type>;

} } // graphene::chain

FC_REFLECT( graphene::chain::destroy_worker_type, (total_burned) )
FC_REFLECT( graphene::chain::vesting_balance_worker_type, (balance) )
FC_REFLECT( graphene::chain::burn_worker_type, (total_burned) )
FC_REFLECT( graphene::chain::issuance_worker_type, (total_issuance) )
FC_REFLECT_TYPENAME( graphene::chain::worker_type )
FC_REFLECT_DERIVED( graphene::chain::worker_object, (graphene::db::object),
                    (name)
                    (describe)
                    (beneficiary)
                    (work_begin_date)
                    (work_end_date)
                    (daily_pay)
                    (worker)
                    (completed)
                  )
