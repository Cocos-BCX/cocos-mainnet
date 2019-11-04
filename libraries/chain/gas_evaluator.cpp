
#include <graphene/chain/database.hpp>
#include <graphene/chain/gas_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/market_object.hpp>
namespace graphene
{
namespace chain
{

void_result update_collateral_for_gas_evaluator::do_evaluate(const operation_type &o)
{
    database &d = db();
    FC_ASSERT(d.find(o.beneficiary));
    auto &index = d.get_index_type<collateral_for_gas_index>().indices().get<by_mortgager_and_beneficiary>();
    auto itr = index.find(boost::make_tuple(o.mortgager, o.beneficiary));
    if (itr != index.end())
        collateral_for_gas = &(*itr);
    return void_result();
};
object_id_result update_collateral_for_gas_evaluator::do_apply(const operation_type &o)
{

    database &d = db();
    share_type temp_supply = 0;
    if (collateral_for_gas)
    {
        auto temp_collateral = o.collateral - collateral_for_gas->collateral;
        auto temp_debt = d.estimation_gas(o.collateral).amount - collateral_for_gas->debt;
        d.modify(*collateral_for_gas, [&](collateral_for_gas_object &cgb) {
            cgb.collateral += temp_collateral;
            cgb.debt += temp_debt;
        });
        const auto &mortgager = o.mortgager(d);
        d.adjust_balance(o.beneficiary, asset(temp_debt, GRAPHENE_ASSET_GAS), true);
        temp_supply = temp_debt;
        if (temp_collateral < 0)
        {
            optional<vesting_balance_id_type> new_vbid = d.deposit_lazy_vesting(
                mortgager.cashback_vb,
                -temp_collateral,
                d.get_global_properties().parameters.cashback_vb_period_seconds,
                o.mortgager,
                "cashback_vb",
                true);
            if (new_vbid.valid())
                d.modify(mortgager, [&new_vbid](account_object &a) { a.cashback_vb = new_vbid; });
        }
        else
        {
            d.adjust_balance(o.mortgager, -temp_collateral);
        }
    }
    else
    {
        collateral_for_gas = &d.create<collateral_for_gas_object>([&](collateral_for_gas_object &cgb) {
            cgb.mortgager = o.mortgager;
            cgb.beneficiary = o.beneficiary;
            cgb.collateral = o.collateral;
            cgb.debt = d.estimation_gas(cgb.collateral).amount;
        });
        d.adjust_balance(o.mortgager, -o.collateral);
        d.adjust_balance(o.beneficiary, asset(collateral_for_gas->debt, GRAPHENE_ASSET_GAS), true);
        temp_supply = collateral_for_gas->debt;
    }
    d.modify(GRAPHENE_ASSET_GAS(d).dynamic_asset_data_id(d), [&](asset_dynamic_data_object &data) {
        data.current_supply += temp_supply;
    });
    return object_id_result(collateral_for_gas->id);
};

} // namespace chain
} // namespace graphene