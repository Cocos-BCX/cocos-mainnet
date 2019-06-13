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
#include <graphene/chain/protocol/types.hpp>

#include <numeric>

namespace graphene { namespace chain {

   /**
    * @defgroup transactions Transactions
    *
    * All transactions are sets of operations that must be applied atomically. Transactions must refer to a recent
    * block that defines the context of the operation so that they assert a known binding to the object id's referenced
    * in the transaction.
    *
    * Rather than specify a full block number, we only specify the lower 16 bits of the block number which means you
    * can reference any block within the last 65,536 blocks which is 3.5 days with a 5 second block interval or 18
    * hours with a 1 second interval.
    *
    * All transactions must expire so that the network does not have to maintain a permanent record of all transactions
    * ever published.  A transaction may not have an expiration date too far in the future because this would require
    * keeping too much transaction history in memory.
    *
    * The block prefix is the first 4 bytes of the block hash of the reference block number, which is the second 4
    * bytes of the @ref block_id_type (the first 4 bytes of the block ID are the block number)
    *
    * Note: A transaction which selects a reference block cannot be migrated between forks outside the period of
    * ref_block_num.time to (ref_block_num.time + rel_exp * interval). This fact can be used to protect market orders
    * which should specify a relatively short re-org window of perhaps less than 1 minute. Normal payments should
    * probably have a longer re-org window to ensure their transaction can still go through in the event of a momentary
    * disruption in service.
    *
    * @note It is not recommended to set the @ref ref_block_num, @ref ref_block_prefix, and @ref expiration
    * fields manually. Call the appropriate overload of @ref set_expiration instead.
    *
    * @{
    */

   /**
    *  @brief groups operations that should be applied atomically
    */
   struct transaction
   {
      /**
       * Least significant 16 bits from the reference block number. If @ref relative_expiration is zero, this field
       * must be zero as well.
       */
      uint16_t           ref_block_num    = 0;// 区块id 第一位 hash[0]&0xFFFF ,与摘要池中编号对应 
      /**
       * The first non-block-number 32-bits of the reference block ID. Recall that block IDs have 32 bits of block
       * number followed by the actual block hash, so this field should be set using the second 32 bits in the
       * @ref block_id_type
       */
      uint32_t           ref_block_prefix = 0;// 等于区块id 第二位　hash[1]

      /**
       * This field specifies the absolute expiration for this transaction.
       */
      fc::time_point_sec expiration;

      vector<operation>  operations;
      extensions_type    extensions;

      /// Calculate the digest for a transaction
      digest_type         digest()const;
      transaction_id_type id()const;
      transaction_id_type id(digest_type hash_value)const;
      tx_hash_type        hash()const{return this->digest();}
      void                validate() const;
      /// Calculate the digest used for signature validation
      digest_type         sig_digest( const chain_id_type& chain_id )const;

      void set_expiration( fc::time_point_sec expiration_time );
      void set_reference_block( const block_id_type& reference_block );

      /// visit all operations
      template<typename Visitor>
      vector<typename Visitor::result_type> visit( Visitor&& visitor )
      {
         vector<typename Visitor::result_type> results;
         for( auto& op : operations )
            results.push_back(op.visit( std::forward<Visitor>( visitor ) ));
         return results;
      }
      template<typename Visitor>
      vector<typename Visitor::result_type> visit( Visitor&& visitor )const
      {
         vector<typename Visitor::result_type> results;
         for( auto& op : operations )
            results.push_back(op.visit( std::forward<Visitor>( visitor ) ));
         return results;
      }
      void get_required_authorities( flat_set<account_id_type>& active, flat_set<account_id_type>& owner, vector<authority>& other )const;

   };

   /**
    *  @brief adds a signature to a transaction
    */
   struct signed_transaction : public transaction
   {
      signed_transaction( const transaction& trx = transaction() )
         : transaction(trx){}

      /** signs and appends to signatures */
      const signature_type& sign( const private_key_type& key, const chain_id_type& chain_id );

      /** returns signature but does not append */
      signature_type sign( const private_key_type& key, const chain_id_type& chain_id )const;

      /**
       *  The purpose of this method is to identify some subset of
       *  @ref available_keys that will produce sufficient signatures
       *  for a transaction.  The result is not always a minimal set of
       *  signatures, but any non-minimal result will still pass
       *  validation.
       */
      set<public_key_type> get_required_signatures(
         const chain_id_type& chain_id,
         const flat_set<public_key_type>& available_keys,
         const std::function<const authority*(account_id_type)>& get_active,
         const std::function<const authority*(account_id_type)>& get_owner,
         uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH
         )const;

      void verify_authority(
         const chain_id_type& chain_id,
         const std::function<const authority*(account_id_type)>& get_active,
         const std::function<const authority*(account_id_type)>& get_owner,flat_set<public_key_type>& sigkeys,
         uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH )const;

      /**
       * This is a slower replacement for get_required_signatures()
       * which returns a minimal set in all cases, including
       * some cases where get_required_signatures() returns a
       * non-minimal set.
       */

      set<public_key_type> minimize_required_signatures(
         const chain_id_type& chain_id,
         const flat_set<public_key_type>& available_keys,
         const std::function<const authority*(account_id_type)>& get_active,
         const std::function<const authority*(account_id_type)>& get_owner,
         uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH
         ) const;

      flat_set<public_key_type> get_signature_keys( const chain_id_type& chain_id )const;

      optional<std::pair<tx_hash_type,object_id_type>> agreed_task={};
      vector<signature_type> signatures;

      /// Removes all operations and signatures
      void clear() { operations.clear(); signatures.clear(); }
   };

   void verify_authority( const vector<operation>& ops, const flat_set<public_key_type>& sigs,
                          const std::function<const authority*(account_id_type)>& get_active,
                          const std::function<const authority*(account_id_type)>& get_owner,
                          uint32_t max_recursion = GRAPHENE_MAX_SIG_CHECK_DEPTH,
                          bool allow_committe = false,
                          const flat_set<account_id_type>& active_aprovals = flat_set<account_id_type>(),
                          const flat_set<account_id_type>& owner_approvals = flat_set<account_id_type>());

   /**
    *  @brief captures the result of evaluating the operations contained in the transaction
    *
    *  When processing a transaction some operations generate
    *  new object IDs and these IDs cannot be known until the
    *  transaction is actually included into a block.  When a
    *  block is produced these new ids are captured and included
    *  with every transaction.  The index in operation_results should
    *  correspond to the same index in operations.
    *
    *  If an operation did not create any new object IDs then 0
    *  should be returned.
    */
   struct processed_transaction : public signed_transaction
   {
      processed_transaction( const signed_transaction& trx = signed_transaction() )
         : signed_transaction(trx){}
      vector<operation_result> operation_results;

      digest_type merkle_digest()const;
      void pack(digest_type::encoder& enc) const;
   };

   /// @} transactions group

} } // graphene::chain

FC_REFLECT( graphene::chain::transaction, (ref_block_num)(ref_block_prefix)(expiration)(operations)(extensions) )
FC_REFLECT_DERIVED( graphene::chain::signed_transaction, (graphene::chain::transaction),(agreed_task)(signatures))
FC_REFLECT_DERIVED( graphene::chain::processed_transaction, (graphene::chain::signed_transaction), (operation_results) )
