
#include <graphene/chain/protocol/operations.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>


namespace graphene { namespace chain {

void create_world_view_operation::validate() const 
{
   FC_ASSERT( !world_view.empty() );
   return;
}

void relate_world_view_operation::validate() const 
{
   return;
}

} } // graphene::chain
