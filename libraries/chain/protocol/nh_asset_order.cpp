
#include <graphene/chain/protocol/operations.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/protocol/nh_asset_order.hpp>

//#include <fc/smart_ref_impl.hpp>

namespace graphene { namespace chain {

void create_nh_asset_order_operation::validate() const 
{

   FC_ASSERT( seller != otcaccount );
   FC_ASSERT( pending_orders_fee.amount >= share_type(0) );
   FC_ASSERT( price.amount >= 0 );
   return;
}

void cancel_nh_asset_order_operation::validate() const 
{

   return;
}

void fill_nh_asset_order_operation::validate() const 
{

   return;
}



} } // graphene::chain
