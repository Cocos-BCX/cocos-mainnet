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
#include <graphene/chain/protocol/base.hpp>
#include <graphene/chain/protocol/account.hpp>
#include <graphene/chain/protocol/assert.hpp>
#include <graphene/chain/protocol/asset_ops.hpp>
#include <graphene/chain/protocol/balance.hpp>
#include <graphene/chain/protocol/custom.hpp>
#include <graphene/chain/protocol/committee_member.hpp>
#include <graphene/chain/protocol/confidential.hpp>
#include <graphene/chain/protocol/fba.hpp>
#include <graphene/chain/protocol/market.hpp>
#include <graphene/chain/protocol/proposal.hpp>
#include <graphene/chain/protocol/transfer.hpp>
#include <graphene/chain/protocol/vesting.hpp>
#include <graphene/chain/protocol/withdraw_permission.hpp>
#include <graphene/chain/protocol/witness.hpp>
#include <graphene/chain/protocol/worker.hpp>

#include <graphene/chain/protocol/contract.hpp>
#include <graphene/chain/protocol/temporary.hpp>

#include <graphene/chain/protocol/nh_asset_creator.hpp>
#include <graphene/chain/protocol/nh_asset_order.hpp>
#include <graphene/chain/protocol/nh_asset.hpp>
#include <graphene/chain/protocol/world_view.hpp>
#include <graphene/chain/protocol/file.hpp>
#include <graphene/chain/protocol/crontab.hpp>

namespace graphene { namespace chain {

   /**
    * @ingroup operations
    *
    * Defines the set of valid operations as a discriminated union type.
    */
   typedef fc::static_variant<
            transfer_operation,           //0     static_variant  operation
            limit_order_create_operation, //1   //自动检测是否有相符的订单，自动调用fill_order_operation撮合
            limit_order_cancel_operation, //2    
            call_order_update_operation,  //3   //调整抵押单
            fill_order_operation,         //4  // 系统私有op
            account_create_operation,     //5
            account_update_operation,     //6
            //account_whitelist_operation,  //7
            account_upgrade_operation,    //8
            //account_transfer_operation, //9  
            asset_create_operation,       //10
            asset_update_operation,       //11
            asset_update_restricted_operation,
            asset_update_bitasset_operation,                         //12     ///更新智能资产
            asset_update_feed_producers_operation,                   //13     /// 修改智能资产喂价账户
            asset_issue_operation,                                   //14    ///转移资产创建者权限
            asset_reserve_operation,                                 //15    ////放弃部分资产，指定资产将被回收，流通量减少，总量不变
            asset_fund_fee_pool_operation,                           //16    ///为费率池提供储备金
            asset_settle_operation,                                  //17    //// 清算bitasset资产
            asset_global_settle_operation,                           //18    /// 在global_settle允许的情况下清算货币交易市场
            asset_publish_feed_operation,                            //19
            witness_create_operation,                                //20 见证人
            witness_update_operation,                                //21
            proposal_create_operation,                               //22
            proposal_update_operation,                               //23
            proposal_delete_operation,                               //24 //提议
            withdraw_permission_create_operation,                    //25 //设置保留金，允许被授权人分期解冻领取保留金
            withdraw_permission_update_operation,                    //26 //更新保留金解冻规则
            withdraw_permission_claim_operation,                     //27 //强行撤回资金
            withdraw_permission_delete_operation,                    //28 //删除保留金资助合约
            committee_member_create_operation,                       //29 //理事会成员创建
            committee_member_update_operation,                       //30 //理事会成员更新
            committee_member_update_global_parameters_operation,     //31 //理事会更新链上运行参数
            vesting_balance_create_operation,                        //32  //保留资金 ，锁定资金
            vesting_balance_withdraw_operation,                      //33  //领取解冻的保留资金
            worker_create_operation,                                 //34
           // custom_operation,                                        //35 无效通用op
           // assert_operation,                                        //36 无必要性的断言op
            balance_claim_operation,                                 //37  取回链初始化时分配的资产
           // override_transfer_operation,                             //38 复写的转账操作
            transfer_to_blind_operation,                             //39   //隐秘转账
            blind_transfer_operation,                                //40
            transfer_from_blind_operation,                           //41
            asset_settle_cancel_operation,  // VIRTUAL               //42 //未有实体的清算取消操作
            asset_claim_fees_operation,                              //43  //资产发行人从资产累计池中取回资产
            fba_distribute_operation,       // VIRTUAL               //44  //未实现
            bid_collateral_operation,                                //45  //智能货币抵押投标
            execute_bid_operation,           // VIRTUAL            //46    // 内部私有op
            contract_create_operation,                               //47
            call_contract_function_operation,                        //48
            temporary_authority_change_operation,                     //49
            register_nh_asset_creator_operation,                     //50
            create_world_view_operation,                             //51
            relate_world_view_operation,                     //52
            create_nh_asset_operation,                               //53
//           relate_nh_asset_operation,                               //54
            delete_nh_asset_operation,                               //55
            transfer_nh_asset_operation,                            // 56
            create_nh_asset_order_operation,                         // 57
            cancel_nh_asset_order_operation,                        // 58
            fill_nh_asset_order_operation,                          // 59
            create_file_operation,                                   // 60
            add_file_relate_account_operation,                       // 61
            file_signature_operation,                                // 62
            relate_parent_file_operation,                     // 63
            revise_contract_operation,                                 //64
            crontab_create_operation,                                // 65
            crontab_cancel_operation,                                // 66
            crontab_recover_operation                                // 67
         > operation;   

   /// @} // operations group

   /**
    *  Appends required authorites to the result vector.  The authorities appended are not the
    *  same as those returned by get_required_auth 
    *
    *  @return a set of required authorities for @ref op
    */
   void operation_get_required_authorities( const operation& op, 
                                            flat_set<account_id_type>& active,
                                            flat_set<account_id_type>& owner,
                                            vector<authority>&  other );

   void operation_validate( const operation& op );

   /**
    *  @brief necessary to support nested operations inside the proposal_create_operation
    */
   struct op_wrapper
   {
      public:
         op_wrapper(const operation& op = operation()):op(op){}
         operation op;
   };

} } // graphene::chain

FC_REFLECT_TYPENAME( graphene::chain::operation )
FC_REFLECT( graphene::chain::op_wrapper, (op) )
