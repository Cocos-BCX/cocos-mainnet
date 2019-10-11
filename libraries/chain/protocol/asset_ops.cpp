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
#include <graphene/chain/protocol/asset_ops.hpp>

namespace graphene { namespace chain {

/**
 *  Valid symbols can contain [A-Z0-9], and '.'
 *  They must start with [A, Z]
 *  They must end with [A, Z]
 *  They can contain a maximum of one '.'
 */
bool is_valid_symbol( const string& symbol )
{
    if( symbol.size() < GRAPHENE_MIN_ASSET_SYMBOL_LENGTH )
        return false;

    if( symbol.substr(0,3) == "BIT" ) 
       return false;

    if( symbol.size() > GRAPHENE_MAX_ASSET_SYMBOL_LENGTH )
        return false;

    if( !isalpha( symbol.front() ) )
        return false;

    if( !isalpha( symbol.back() ) )
        return false;

    bool dot_already_present = false;
    for( const auto c : symbol )
    {
        if( (isalpha( c ) && isupper( c )) || isdigit(c) )
            continue;

        if( c == '.' )
        {
            if( dot_already_present )
                return false;

            dot_already_present = true;
            continue;
        }

        return false;
    }

    return true;
}

share_type asset_issue_operation::calculate_fee(const fee_parameters_type& k)const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(memo), k.price_per_kbyte );
}

share_type asset_create_operation::calculate_fee(const asset_create_operation::fee_parameters_type& param)const
{
   auto core_fee_required = param.long_symbol; 

   switch(symbol.size()) {
      case 3: core_fee_required = param.symbol3;
          break;
      case 4: core_fee_required = param.symbol4;
          break;
      default:
          break;
   }

   // common_options contains several lists and a string. Charge fees for its size
   core_fee_required += calculate_data_fee( fc::raw::pack_size(*this), param.price_per_kbyte );

   return core_fee_required;
}

void  asset_create_operation::validate()const
{
   FC_ASSERT( is_valid_symbol(symbol) );
   common_options.validate();
   if( common_options.issuer_permissions & (disable_force_settle|global_settle) )
      FC_ASSERT( bitasset_opts.valid() );
  /*
    取消二元预测市场
   if( is_prediction_market )
   {
      FC_ASSERT( bitasset_opts.valid(), "Cannot have a User-Issued Asset implement a prediction market." );
      FC_ASSERT( common_options.issuer_permissions & global_settle );
   }
  */
   if( bitasset_opts ) bitasset_opts->validate();
   FC_ASSERT(common_options.max_supply.value*std::pow(10,precision)<share_type::max());
   FC_ASSERT(precision <= 12);
}

void asset_update_operation::validate()const
{

   if( new_issuer )
      FC_ASSERT(issuer != *new_issuer);
   new_options.validate();
}

share_type asset_update_operation::calculate_fee(const asset_update_operation::fee_parameters_type& k)const
{
   return k.fee + calculate_data_fee( fc::raw::pack_size(*this), k.price_per_kbyte );
}


void asset_publish_feed_operation::validate()const
{

   feed.validate();

   // maybe some of these could be moved to feed.validate()
   if( !feed.settlement_price.is_null() )
   {
      feed.settlement_price.validate();
   }

   FC_ASSERT( !feed.settlement_price.is_null() );
   FC_ASSERT( feed.is_for( asset_id ) );
}

void asset_reserve_operation::validate()const
{

   FC_ASSERT( amount_to_reserve.amount.value <= GRAPHENE_MAX_SHARE_SUPPLY );
   FC_ASSERT( amount_to_reserve.amount.value > 0 );
}

void asset_issue_operation::validate()const
{

   FC_ASSERT( asset_to_issue.amount.value <= GRAPHENE_MAX_SHARE_SUPPLY );
   FC_ASSERT( asset_to_issue.amount.value > 0 );
   //FC_ASSERT( asset_to_issue.asset_id != asset_id_type(0) ); //nico 
}

void asset_settle_operation::validate() const
{

   FC_ASSERT( amount.amount >= 0 );
}

void asset_update_bitasset_operation::validate() const
{

   new_options.validate();
}

void asset_update_feed_producers_operation::validate() const
{

}

void asset_global_settle_operation::validate()const
{

   FC_ASSERT( asset_to_settle == settle_price.base.asset_id );
}

void bitasset_options::validate() const
{
   FC_ASSERT(minimum_feeds > 0);
   FC_ASSERT(force_settlement_offset_percent <= GRAPHENE_100_PERCENT);
   FC_ASSERT(maximum_force_settlement_volume <= GRAPHENE_100_PERCENT);
}

void asset_options::validate()const
{
   FC_ASSERT( max_supply > 0 );
   FC_ASSERT( max_supply <= GRAPHENE_MAX_SHARE_SUPPLY );
   FC_ASSERT( market_fee_percent <= GRAPHENE_100_PERCENT );
   FC_ASSERT( max_market_fee >= 0 && max_market_fee <= GRAPHENE_MAX_SHARE_SUPPLY );
   // There must be no high bits in permissions whose meaning is not known.
   FC_ASSERT( !(issuer_permissions & ~ASSET_ISSUER_PERMISSION_MASK),"issuer_permissions:${issuer_permissions},ASSET_ISSUER_PERMISSION_MASK:${ASSET_ISSUER_PERMISSION_MASK}",
   ("issuer_permissions",issuer_permissions)("ASSET_ISSUER_PERMISSION_MASK",ASSET_ISSUER_PERMISSION_MASK));
   // The global_settle flag may never be set (this is a permission only)
   FC_ASSERT( !(flags & global_settle) );
   if(core_exchange_rate)
      FC_ASSERT(core_exchange_rate->quote.asset_id==asset_id_type()&&core_exchange_rate->quote.amount>0&&core_exchange_rate->base.amount>0);
   // the witness_fed and committee_fed flags cannot be set simultaneously
   FC_ASSERT( (flags & (witness_fed_asset | committee_fed_asset)) != (witness_fed_asset | committee_fed_asset) );
}

void asset_claim_fees_operation::validate()const {

   FC_ASSERT( amount_to_claim.amount > 0 );
}

void  asset_update_restricted_operation::validate()const
{
      FC_ASSERT(restricted_type!=restricted_enum::all_restricted);
      
}
} } // namespace graphene::chain
