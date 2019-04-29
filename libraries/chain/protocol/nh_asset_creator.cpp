
#include <graphene/chain/protocol/operations.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/protocol/nh_asset_creator.hpp>
//#include <fc/smart_ref_impl.hpp>

namespace graphene { namespace chain {

void register_nh_asset_creator_operation::validate() const 
{
   FC_ASSERT( fee.amount >= share_type(0) );
   return;
}

} } // graphene::chain
