
#pragma once
#include <graphene/chain/protocol/base.hpp>

namespace graphene
{
namespace chain
{

// 创建出售单
struct create_nh_asset_order_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = GRAPHENE_BLOCKCHAIN_PRECISION;
   };
   account_id_type seller;                                // 挂单人
   account_id_type otcaccount;                            // 第三方OTC账户
   asset pending_orders_fee;                              // 挂单费用
   nh_asset_id_type nh_asset;                             // 道具ID
   string memo;                                           // 附加信息
   asset price;                                           // 挂单价格
   time_point_sec expiration = time_point_sec::maximum(); // 过期时间
   account_id_type fee_payer() const { return seller; }
   void validate() const;
};

// 取消出售单
struct cancel_nh_asset_order_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = 0;
   };
   nh_asset_order_id_type order; // 出售单ID
   /** must be order->seller */
   account_id_type fee_paying_account; // 出售者
   extensions_type extensions;         // 扩展信息

   account_id_type fee_payer() const { return fee_paying_account; }
   void validate() const;
};

// 购买订单道具
struct fill_nh_asset_order_operation : public base_operation
{
   struct fee_parameters_type
   {
      uint64_t fee = 0;
   };
   nh_asset_order_id_type order; // 出售单ID
   /** must be order->seller */
   account_id_type fee_paying_account; // 购买者
   account_id_type seller;             // 出售者
   nh_asset_id_type nh_asset;          // 购买的游戏道具
   string price_amount;
   asset_id_type price_asset_id;
   string price_asset_symbol;
   extensions_type extensions; // 扩展信息

   account_id_type fee_payer() const { return fee_paying_account; }
   void validate() const;
};

} // namespace chain
} // namespace graphene

FC_REFLECT(graphene::chain::create_nh_asset_order_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::create_nh_asset_order_operation, (seller)(otcaccount)(pending_orders_fee)(nh_asset)(memo)(price)(expiration))

FC_REFLECT(graphene::chain::cancel_nh_asset_order_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::cancel_nh_asset_order_operation, (order)(fee_paying_account)(extensions))

FC_REFLECT(graphene::chain::fill_nh_asset_order_operation::fee_parameters_type, (fee))

FC_REFLECT(graphene::chain::fill_nh_asset_order_operation, (order)(fee_paying_account)(seller)(nh_asset)(price_amount)(price_asset_id)(price_asset_symbol)(extensions))