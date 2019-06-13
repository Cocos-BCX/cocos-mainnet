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
#include <graphene/chain/nh_asset_evaluator.hpp>
#include <graphene/chain/nh_asset_object.hpp>
#include <graphene/chain/world_view_object.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/nh_asset_creator_object.hpp>

#include <graphene/chain/protocol/fee_schedule.hpp>
#include <graphene/chain/exceptions.hpp>

#include <fc/smart_ref_impl.hpp>

#include <fc/log/logger.hpp>

namespace graphene
{
namespace chain
{

void_result create_nh_asset_evaluator::do_evaluate(const create_nh_asset_operation &o)
{
    database &d = db();
    //校验创建者是否为游戏开发者
    const auto &nh_asset_creator_idx_by_nh_asset_creator = d.get_index_type<nh_asset_creator_index>().indices().get<by_nh_asset_creator>();
    const auto &nh_asset_creator_idx = nh_asset_creator_idx_by_nh_asset_creator.find(o.fee_paying_account);
    FC_ASSERT(nh_asset_creator_idx != nh_asset_creator_idx_by_nh_asset_creator.end(), "You’re not a nh asset creater, so you can’t create.");
    FC_ASSERT(find(nh_asset_creator_idx->world_view.begin(), nh_asset_creator_idx->world_view.end(), o.world_view) != nh_asset_creator_idx->world_view.end(), "You didn't related to this world view.");
    //校验资产是否存在
    const auto &asset_idx_by_symbol = d.get_index_type<asset_index>().indices().get<by_symbol>();
    const auto &asset_idx = asset_idx_by_symbol.find(o.asset_id);
    FC_ASSERT(asset_idx != asset_idx_by_symbol.end(), "The asset id is not exist.");
    //校验游戏版本是否存在
    const auto &version_idx_by_symbol = d.get_index_type<world_view_index>().indices().get<by_world_view>();
    const auto &ver_idx = version_idx_by_symbol.find(o.world_view);
    FC_ASSERT(ver_idx != version_idx_by_symbol.end(), "The world view is not exist.");
    return void_result();
}

object_id_result create_nh_asset_evaluator::do_apply(const create_nh_asset_operation &o)
{
    try
    {
        database &d = db();
        //创建游戏道具对象
        const nh_asset_object &nh_asset_obj = d.create<nh_asset_object>([&](nh_asset_object &nh_asset) {
            if (o.owner == account_id_type())
                nh_asset.nh_asset_owner = o.fee_paying_account;
            else
                nh_asset.nh_asset_owner = o.owner;
            nh_asset.nh_asset_creator = o.fee_paying_account;
			nh_asset.nh_asset_active = nh_asset.nh_asset_owner;
			nh_asset.dealership = nh_asset.nh_asset_owner;
            nh_asset.asset_qualifier = o.asset_id;
            nh_asset.world_view = o.world_view;
            nh_asset.base_describe =o.base_describe;
            nh_asset.create_time = d.head_block_time();
            nh_asset.get_hash();
        });

        return nh_asset_obj.id;
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result delete_nh_asset_evaluator::do_evaluate(const delete_nh_asset_operation &o)
{
    database &d = db();
    //校验游戏道具是否存在
    FC_ASSERT(d.find_object(o.nh_asset), "Could not find nh asset matching ${nh_asset}", ("nh_asset", o.nh_asset));
    FC_ASSERT(o.nh_asset(d).nh_asset_owner == o.fee_paying_account, "You’re not the item’s owner，so you can’t delete it.");
    return void_result();
}

void_result delete_nh_asset_evaluator::do_apply(const delete_nh_asset_operation &o)
{
    try
    {
        database &d = db();
        //删除游戏道具对象
        d.remove(o.nh_asset(d));

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result transfer_nh_asset_evaluator::do_evaluate(const transfer_nh_asset_operation &o)
{
    database &d = db();
    //校验游戏道具是否存在
    const auto &nht=o.nh_asset(d);
    //FC_ASSERT( o.nh_asset(d).nh_asset_owner == o.fee_paying_account , "You’re not the item’s owner，so you can’t delete it." );
    //校验交易人是否为道具所有人
    FC_ASSERT(nht.nh_asset_owner == o.from, "You’re not the item’s owner，so you can’t transfer it.");
    FC_ASSERT(nht.nh_asset_owner==nht.nh_asset_active&&nht.nh_asset_owner==nht.dealership);
    return void_result();
}

void_result transfer_nh_asset_evaluator::do_apply(const transfer_nh_asset_operation &o)
{
    try
    {
        database &d = db();
        //修改游戏道具对象的属性--道具所有人
        d.modify(o.nh_asset(d), [&](nh_asset_object &g) {
            g.nh_asset_owner = o.to;
			g.nh_asset_active = o.to;
			g.dealership = o.to;
        });
        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

void_result relate_nh_asset_evaluator::do_evaluate(const operation_type &o)
{
    database &d = db();
    //校验游戏道具是否存在
    //   FC_ASSERT(d.find_object(o.nh_asset), "Could not find nh asset matching ${nh_asset}", ("nh_asset", o.nh_asset));
    //校验交易人是否为道具所有人
    //   FC_ASSERT(o.nh_asset(d).nh_asset_owner == o.from, "You’re not the item’s owner，so you can’t transfer it.");

    FC_ASSERT(d.find_object(o.parent), "Could not find nh asset matching ${nh_asset}", ("nh_asset", o.parent));
    FC_ASSERT(d.find_object(o.child), "Could not find nh asset matching ${nh_asset}", ("nh_asset", o.child));
    //校验合约是否存在
    FC_ASSERT(d.find_object(o.contract), "Could not find contract matching ${contract}", ("contract", o.contract));
    //校验操作者是否为道具创建者
    FC_ASSERT(o.parent(d).nh_asset_creator == o.nh_asset_creator, "You’re not the parent item’s creater, so you can’t relate it.");
    FC_ASSERT(o.child(d).nh_asset_creator == o.nh_asset_creator, "You’re not the child item’s creater, so you can’t relate it.");

    const nh_asset_object &gio = o.child(d);
    const auto &iter = gio.parent.find(o.contract);
    if (o.relate && gio.parent.count(o.contract))
    {
        FC_ASSERT(find(iter->second.begin(), iter->second.end(), o.parent) == iter->second.end(), "The parent item and child item had be related.");
    }
    if (!o.relate)
    {
        FC_ASSERT(gio.parent.count(o.contract) == 1, "The child item's parent dosen't contain this contract.");
        FC_ASSERT(find(iter->second.begin(), iter->second.end(), o.parent) != iter->second.end(), "The parent item and child item did not relate.");
    }
    return void_result();
}

void_result relate_nh_asset_evaluator::do_apply(const operation_type &o)
{
    try
    {
        database &d = db();
        if (o.relate) // zhangfan 关联操作
        {
            const nh_asset_object &gio_child = o.child(d);
            if (gio_child.parent.count(o.contract)) // zhangfan 该道具的父集中已经存在该合约添加的关联关系
            {
                //修改游戏道具对象的属性--父集
                d.modify(o.child(d), [&](nh_asset_object &g) {
                    g.parent[o.contract].push_back(o.parent);
                });
                //修改游戏道具对象的属性--子集
                d.modify(o.parent(d), [&](nh_asset_object &g) {
                    g.child[o.contract].push_back(o.child);
                });
            }
            // zhangfan 该道具中不存在该合约添加的关联关系
            else
            {
                //修改游戏道具对象的属性--父集
                d.modify(o.child(d), [&](nh_asset_object &g) {
                    g.parent.insert(std::make_pair(o.contract, vector<nh_asset_id_type>(1, o.parent)));
                });
                //修改游戏道具对象的属性--子集
                d.modify(o.parent(d), [&](nh_asset_object &g) {
                    g.child.insert(std::make_pair(o.contract, vector<nh_asset_id_type>(1, o.child)));
                });
            }
        }
        else // 取消关联操作
        {
            //修改游戏道具对象的属性--从父集中删除父道具
            d.modify(o.child(d), [&](nh_asset_object &g) {
                auto &vec = g.parent[o.contract];
                vec.erase(find(vec.begin(), vec.end(), o.parent));
            });

            //修改游戏道具对象的属性--从子集中删除子道具
            d.modify(o.parent(d), [&](nh_asset_object &g) {
                auto &vec = g.child[o.contract];
                vec.erase(find(vec.begin(), vec.end(), o.child));
            });
        }

        return void_result();
    }
    FC_CAPTURE_AND_RETHROW((o))
}

} // namespace chain
} // namespace graphene
