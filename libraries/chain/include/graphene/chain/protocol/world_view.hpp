
#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{

//create world view
struct create_world_view_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    account_id_type fee_paying_account; // creator
    string world_view;                  // the world view name
    account_id_type fee_payer() const { return fee_paying_account; }
    void validate() const;
};

// relate a nth creator to a world view
struct relate_world_view_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    account_id_type related_account; // the nht creator account who whill be related
    string world_view;                  // the world view
    account_id_type view_owner;         // the world view's creator
    account_id_type fee_payer() const { return view_owner; }
    void validate() const;
};

} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::create_world_view_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::create_world_view_operation, (fee_paying_account)(world_view))

FC_REFLECT(graphene::chain::relate_world_view_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::relate_world_view_operation, (related_account)(world_view)(view_owner))
