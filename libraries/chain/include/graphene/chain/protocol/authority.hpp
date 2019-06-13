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

namespace graphene { namespace chain {

   /**
    *  @class authority
    *  @brief Identifies a weighted set of keys and accounts that must approve operations.
    */
   struct authority
   {
      authority(){}
      template<class ...Args>
      authority(uint32_t threshhold, Args... auths)
         : weight_threshold(threshhold)
      {
         add_authorities(auths...);
      }

      enum classification
      {
         /** the key that is authorized to change owner, active, and voting keys */
         owner  = 0,
         /** the key that is able to perform normal operations */
         active = 1,
         key    = 2
      };
      void add_authority( const public_key_type& k, weight_type w )
      {
         key_auths[k] = w;
      }
      void add_authority( const address& k, weight_type w )
      {
         address_auths[k] = w;
      }
      void add_authority( account_id_type k, weight_type w )
      {
         account_auths[k] = w;
      }
      bool is_impossible()const
      {
         uint64_t auth_weights = 0;
         for( const auto& item : account_auths ) auth_weights += item.second;
         for( const auto& item : key_auths ) auth_weights += item.second;
         for( const auto& item : address_auths ) auth_weights += item.second;
         return auth_weights < weight_threshold;
      }

      template<typename AuthType>
      void add_authorities(AuthType k, weight_type w)
      {
         add_authority(k, w);
      }
      template<typename AuthType, class ...Args>
      void add_authorities(AuthType k, weight_type w, Args... auths)
      {
         add_authority(k, w);
         add_authorities(auths...);
      }

      vector<public_key_type> get_keys() const
      {
         vector<public_key_type> result;
         result.reserve( key_auths.size() );
         for( const auto& k : key_auths )
            result.push_back(k.first);
         return result;
      }
      vector<address> get_addresses() const
      {
         vector<address> result;
         result.reserve( address_auths.size() );
         for( const auto& k : address_auths )
            result.push_back(k.first);
         return result;
      }


      friend bool operator == ( const authority& a, const authority& b )
      {
         return (a.weight_threshold == b.weight_threshold) &&
                (a.account_auths == b.account_auths) &&
                (a.key_auths == b.key_auths) &&
                (a.address_auths == b.address_auths); 
      }
      uint32_t num_auths()const { return account_auths.size() + key_auths.size() + address_auths.size(); }
      void     clear() { account_auths.clear(); key_auths.clear(); }

      static authority null_authority()
      {
         return authority( 1, GRAPHENE_NULL_ACCOUNT, 1 );
      }

      uint32_t                              weight_threshold = 0;
      flat_map<account_id_type,weight_type> account_auths;
      flat_map<public_key_type,weight_type> key_auths;
      /** needed for backward compatibility only */
      flat_map<address,weight_type>         address_auths;
   };

/**
 * Add all account members of the given authority to the given flat_set.
 */
void add_authority_accounts(
   flat_set<account_id_type>& result,
   const authority& a
   );

} } // namespace graphene::chain

FC_REFLECT( graphene::chain::authority, (weight_threshold)(account_auths)(key_auths)(address_auths) )
FC_REFLECT_ENUM( graphene::chain::authority::classification, (owner)(active)(key) )
