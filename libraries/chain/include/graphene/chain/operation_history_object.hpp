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
#include <graphene/chain/protocol/operations.hpp>
#include <graphene/db/object.hpp>
#include <boost/multi_index/composite_key.hpp>

namespace graphene { namespace chain {

   /**
    * @brief tracks the history of all logical operations on blockchain state
    * @ingroup object
    * @ingroup implementation
    *
    *  All operations and virtual operations result in the creation of an
    *  operation_history_object that is maintained on disk as a stack.  Each
    *  real or virtual operation is assigned a unique ID / sequence number that
    *  it can be referenced by.
    *
    *  @note  by default these objects are not tracked, the account_history_plugin must
    *  be loaded fore these objects to be maintained.
    *
    *  @note  this object is READ ONLY it can never be modified
    */
   class operation_history_object : public abstract_object<operation_history_object>
   {
      public:
         static const uint8_t space_id = protocol_ids;
         static const uint8_t type_id  = operation_history_object_type;

         operation_history_object( const operation& o ):op(o){}
         operation_history_object(){}

         operation         op;
         operation_result  result;
         /** the block that caused this operation */
         uint32_t          block_num = 0;
         /** the transaction in the block */
         uint16_t          trx_in_block = 0;
         /** the operation within the transaction */
         uint16_t          op_in_trx = 0;
         /** any virtual operations implied by operation in block */
         uint16_t          virtual_op = 0;
   };

   /**
    *  @brief a node in a linked list of operation_history_objects
    *  @ingroup implementation
    *  @ingroup object
    *
    *  Account history is important for users and wallets even though it is
    *  not part of "core validation".   Account history is maintained as
    *  a linked list stored on disk in a stack.  Each account will point to the
    *  most recent account history object by ID.  When a new operation relativent
    *  to that account is processed a new account history object is allcoated at
    *  the end of the stack and intialized to point to the prior object.
    *
    *  This data is never accessed as part of chain validation and therefore
    *  can be kept on disk as a memory mapped file.  Using a memory mapped file
    *  will help the operating system better manage / cache / page files and
    *  also accelerates load time.
    *
    *  When the transaction history for a particular account is requested the
    *  linked list can be traversed with relatively effecient disk access because
    *  of the use of a memory mapped stack.
    */
   class account_transaction_history_object :  public abstract_object<account_transaction_history_object>
   {
      public:
         static const uint8_t space_id = implementation_ids;
         static const uint8_t type_id  = impl_account_transaction_history_object_type;
         account_id_type                      account; /// the account this operation applies to
         operation_history_id_type            operation_id;
         uint32_t                             sequence = 0; /// the operation position within the given account
         account_transaction_history_id_type  next;

         //std::pair<account_id_type,operation_history_id_type>  account_op()const  { return std::tie( account, operation_id ); }
         //std::pair<account_id_type,uint32_t>                   account_seq()const { return std::tie( account, sequence );     }
   };

   struct by_id;

   typedef multi_index_container<
      operation_history_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >
      >
   > operation_history_multi_index_type;

   typedef generic_index<operation_history_object, operation_history_multi_index_type> operation_history_index;

   struct by_seq;
   struct by_op;
   struct by_opid;

   typedef multi_index_container<
      account_transaction_history_object,
      indexed_by<
         ordered_unique< tag<by_id>, member< object, object_id_type, &object::id > >,
         ordered_unique< tag<by_seq>,
            composite_key< account_transaction_history_object,
               member< account_transaction_history_object, account_id_type, &account_transaction_history_object::account>,
               member< account_transaction_history_object, uint32_t, &account_transaction_history_object::sequence>
            >
         >,
         ordered_unique< tag<by_op>,
            composite_key< account_transaction_history_object,
               member< account_transaction_history_object, account_id_type, &account_transaction_history_object::account>,
               member< account_transaction_history_object, operation_history_id_type, &account_transaction_history_object::operation_id>
            >
         >,
         ordered_non_unique< tag<by_opid>,
            member< account_transaction_history_object, operation_history_id_type, &account_transaction_history_object::operation_id>
         >
      >
   > account_transaction_history_multi_index_type;

   typedef generic_index<account_transaction_history_object, account_transaction_history_multi_index_type> account_transaction_history_index;


} } // graphene::chain

FC_REFLECT_DERIVED( graphene::chain::operation_history_object, (graphene::chain::object),
                    (op)(result)(block_num)(trx_in_block)(op_in_trx)(virtual_op) )

FC_REFLECT_DERIVED( graphene::chain::account_transaction_history_object, (graphene::chain::object),
                    (account)(operation_id)(sequence)(next) )
