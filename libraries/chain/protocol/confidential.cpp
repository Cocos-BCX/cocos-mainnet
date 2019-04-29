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
#include <graphene/chain/protocol/confidential.hpp>
#include <graphene/chain/confidential_evaluator.hpp>
#include <graphene/chain/database.hpp>

#include <fc/crypto/base58.hpp>
#include <fc/io/raw.hpp>
#include <fc/crypto/elliptic.hpp>

namespace graphene { namespace chain {

void transfer_to_blind_operation::validate()const
{
   FC_ASSERT( fee.amount >= share_type(0) );
   FC_ASSERT( amount.amount > 0 );

   vector<commitment_type> in;
   vector<commitment_type> out(outputs.size());
   int64_t                 net_public = amount.amount.value;
   for( uint32_t i = 0; i < out.size(); ++i )
   {
      out[i] = outputs[i].commitment;
      /// require all outputs to be sorted prevents duplicates AND prevents implementations
      /// from accidentally leaking information by how they arrange commitments.
      if( i > 0 ) FC_ASSERT( out[i-1] < out[i], "all outputs must be sorted by commitment id" );
      FC_ASSERT( !outputs[i].owner.is_impossible() );
   }
   FC_ASSERT( out.size(), "there must be at least one output" );

   auto public_c = fc::ecc::blind(blinding_factor,net_public);

   FC_ASSERT( fc::ecc::verify_sum( {public_c}, out, 0 ), "", ("net_public",net_public) );

   if( outputs.size() > 1 )
   {
      for( auto out : outputs )
      {
         auto info = fc::ecc::range_get_info( out.range_proof );
         FC_ASSERT( info.max_value <= GRAPHENE_MAX_SHARE_SUPPLY );
      }
   }
}

share_type transfer_to_blind_operation::calculate_fee( const fee_parameters_type& k )const
{
    return k.fee + outputs.size() * k.price_per_output;
}


void transfer_from_blind_operation::validate()const
{
   FC_ASSERT( amount.amount > 0 );
   FC_ASSERT( fee.amount >= share_type(0) );
   FC_ASSERT( inputs.size() > 0 );
   FC_ASSERT( amount.asset_id == fee.asset_id );


   vector<commitment_type> in(inputs.size());
   vector<commitment_type> out;
   int64_t                 net_public = fee.amount.value + amount.amount.value;
   out.push_back( fc::ecc::blind( blinding_factor, net_public ) );
   for( uint32_t i = 0; i < in.size(); ++i )
   {
      in[i] = inputs[i].commitment;
      /// by requiring all inputs to be sorted we also prevent duplicate commitments on the input
      if( i > 0 ) FC_ASSERT( in[i-1] < in[i], "all inputs must be sorted by commitment id" );
   }
   FC_ASSERT( in.size(), "there must be at least one input" );
   FC_ASSERT( fc::ecc::verify_sum( in, out, 0 ) );
}


/**
 *  If fee_payer = temp_account_id, then the fee is paid by the surplus balance of inputs-outputs and
 *  100% of the fee goes to the network.
 */
account_id_type blind_transfer_operation::fee_payer()const
{
   return GRAPHENE_TEMP_ACCOUNT;
}


/**
 *  This method can be computationally intensive because it verifies that input commitments - output commitments add up to 0
 */
void blind_transfer_operation::validate()const
{ try {
   vector<commitment_type> in(inputs.size());
   vector<commitment_type> out(outputs.size());
   int64_t                 net_public = fee.amount.value;//from_amount.value - to_amount.value;
   for( uint32_t i = 0; i < in.size(); ++i )
   {
      in[i] = inputs[i].commitment;
      /// by requiring all inputs to be sorted we also prevent duplicate commitments on the input
      if( i > 0 ) FC_ASSERT( in[i-1] < in[i] );
   }
   for( uint32_t i = 0; i < out.size(); ++i )
   {
      out[i] = outputs[i].commitment;
      if( i > 0 ) FC_ASSERT( out[i-1] < out[i] );
      FC_ASSERT( !outputs[i].owner.is_impossible() );
   }
   FC_ASSERT( in.size(), "there must be at least one input" );
   FC_ASSERT( fc::ecc::verify_sum( in, out, net_public ), "", ("net_public", net_public) );

   if( outputs.size() > 1 )
   {
      for( auto out : outputs )
      {
         auto info = fc::ecc::range_get_info( out.range_proof );
         FC_ASSERT( info.max_value <= GRAPHENE_MAX_SHARE_SUPPLY );
      }
   }
   FC_ASSERT( fc::ecc::verify_sum( in, out, net_public ), "", ("net_public", net_public) );
} FC_CAPTURE_AND_RETHROW( (*this) ) }

share_type blind_transfer_operation::calculate_fee( const fee_parameters_type& k )const
{
    return k.fee + outputs.size() * k.price_per_output;
}




/**
 *  Packs *this then encodes as base58 encoded string.
 */
stealth_confirmation::operator string()const
{
   return fc::to_base58( fc::raw::pack( *this ) );
}
/**
 * Unpacks from a base58 string
 */
stealth_confirmation::stealth_confirmation( const std::string& base58 )
{
   *this = fc::raw::unpack<stealth_confirmation>( fc::from_base58( base58 ) );
}



} } // graphene::chain
