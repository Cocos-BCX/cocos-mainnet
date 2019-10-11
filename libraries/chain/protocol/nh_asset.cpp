
#include <graphene/chain/protocol/operations.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>

#include <graphene/chain/protocol/nh_asset.hpp>

namespace graphene
{
namespace chain
{

void create_nh_asset_operation::validate() const
{
    FC_ASSERT( !base_describe.empty(), "the base describe can't be empty" );
    return;
}
void delete_nh_asset_operation::validate() const
{
    return;
}
void transfer_nh_asset_operation::validate() const
{
    FC_ASSERT(from != to);
    return;
}

void relate_nh_asset_operation::validate() const
{
    FC_ASSERT( parent != child );
    return;
}

} // namespace chain
} // namespace graphene
