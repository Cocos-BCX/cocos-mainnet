
#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{

//create non homogenesis asset
struct create_nh_asset_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    account_id_type fee_paying_account; // the creator
    account_id_type owner;              // the non homogenesis asset's owner
    string asset_id;  // the qualifier
    string world_view;  // the world view
    string base_describe; // base describe of the non homogenesis asset
    account_id_type fee_payer() const { return fee_paying_account; }
    void validate() const;
};

//delete non homogenesis asset
struct delete_nh_asset_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    account_id_type fee_paying_account; // the operator who must be the non homogenesis asset's owner
    nh_asset_id_type nh_asset;        // the non homogenesis asset's id which will be delete
    account_id_type fee_payer() const { return fee_paying_account; }
    void validate() const;
};

// transfer non homogenesis asset
struct transfer_nh_asset_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    account_id_type from;        // the operator who will transfer non homogenesis asset to another one
    account_id_type to;          // the account who will receive the non homogenesis asset
    nh_asset_id_type nh_asset; // the non homogenesis asset's id
    account_id_type fee_payer() const { return from; }
    void validate() const;
};
//relate non homogenesis asset
struct relate_nh_asset_operation : public base_operation
{
    struct fee_parameters_type
    {
        uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
    };
    account_id_type nh_asset_creator; // the operator who must be the child non homogenesis asset's owner
    nh_asset_id_type parent;     // the parent non homogenesis asset's id
    nh_asset_id_type child;      // the child non homogenesis asset's id
    contract_id_type contract;    // related contract's id
    bool relate;                  // if true, they will be related, otherwise will be unrelated
    account_id_type fee_payer() const { return nh_asset_creator; }
    void validate() const;
};
} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::create_nh_asset_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::create_nh_asset_operation, (fee_paying_account)(owner)(asset_id)(world_view)(base_describe))

FC_REFLECT(graphene::chain::delete_nh_asset_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::delete_nh_asset_operation, (fee_paying_account)(nh_asset))

FC_REFLECT(graphene::chain::transfer_nh_asset_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::transfer_nh_asset_operation, (from)(to)(nh_asset))

FC_REFLECT( graphene::chain::relate_nh_asset_operation::fee_parameters_type, (fee) )

FC_REFLECT( graphene::chain::relate_nh_asset_operation, (nh_asset_creator)(parent)(child)(relate) )
