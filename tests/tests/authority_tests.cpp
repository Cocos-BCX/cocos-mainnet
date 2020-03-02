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

#include <boost/test/unit_test.hpp>

#include <graphene/chain/database.hpp>
#include <graphene/chain/protocol/protocol.hpp>
#include <graphene/chain/exceptions.hpp>

#include <graphene/chain/account_object.hpp>
#include <graphene/chain/asset_object.hpp>
#include <graphene/chain/committee_member_object.hpp>
#include <graphene/chain/proposal_object.hpp>

#include <graphene/db/simple_index.hpp>

#include <fc/crypto/digest.hpp>
#include "../common/database_fixture.hpp"

using namespace graphene::chain;
using namespace graphene::chain::test;

BOOST_FIXTURE_TEST_SUITE(authority_tests, database_fixture)

BOOST_AUTO_TEST_CASE(simple_single_signature)
{
    try
    {
        try
        {
            fc::ecc::private_key nathan_key = fc::ecc::private_key::generate();
            const account_object &nathan = create_account("nathan", nathan_key.get_public_key());
            const asset_object &core = asset_id_type()(*db);
            auto old_balance = fund(nathan);
            transfer_operation op;
            op.from = nathan.id;
            op.to = account_id_type();
            op.amount = core.amount(500);
            trx.operations.push_back(op);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);

            BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 500);
        }
        catch (fc::exception &e)
        {
            edump((e.to_detail_string()));
            throw;
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_AUTO_TEST_CASE(any_two_of_three)
{
    try
    {
        fc::ecc::private_key nathan_key1 = fc::ecc::private_key::regenerate(fc::digest("key1"));
        fc::ecc::private_key nathan_key2 = fc::ecc::private_key::regenerate(fc::digest("key2"));
        fc::ecc::private_key nathan_key3 = fc::ecc::private_key::regenerate(fc::digest("key3"));
        const account_object &nathan = create_account("nathan", nathan_key1.get_public_key());
        const asset_object &core = asset_id_type()(*db);
        auto old_balance = fund(nathan);

        try
        {
            account_update_operation op;
            op.account = nathan.id;
            op.active = authority(2, public_key_type(nathan_key1.get_public_key()), 1, public_key_type(nathan_key2.get_public_key()), 1, public_key_type(nathan_key3.get_public_key()), 1);
            op.owner = *op.active;
            trx.operations.push_back(op);
            sign(trx, nathan_key1);
            PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
            trx.operations.clear();
            trx.signatures.clear();
        }
        FC_CAPTURE_AND_RETHROW((nathan.active))

        transfer_operation op;
        op.from = nathan.id;
        op.to = account_id_type();
        op.amount = core.amount(500);
        trx.operations.push_back(op);
        sign(trx, nathan_key1);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        sign(trx, nathan_key2);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 500);

        trx.signatures.clear();
        sign(trx, nathan_key2);
        sign(trx, nathan_key3);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1000);

        trx.signatures.clear();
        sign(trx, nathan_key1);
        sign(trx, nathan_key3);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1500);

        trx.signatures.clear();
        //sign(trx, fc::ecc::private_key::generate());
        sign(trx, nathan_key3);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        BOOST_CHECK_EQUAL(get_balance(nathan, core), old_balance - 1500);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(recursive_accounts)
{
    try
    {
        fc::ecc::private_key parent1_key = fc::ecc::private_key::generate();
        fc::ecc::private_key parent2_key = fc::ecc::private_key::generate();
        const auto &core = asset_id_type()(*db);

        BOOST_TEST_MESSAGE("Creating parent1 and parent2 accounts");
        const account_object &parent1 = create_account("parent1", parent1_key.get_public_key());
        const account_object &parent2 = create_account("parent2", parent2_key.get_public_key());

        BOOST_TEST_MESSAGE("Creating child account that requires both parent1 and parent2 to approve");
        {
            auto make_child_op = make_account("child");
            make_child_op.owner = authority(2, account_id_type(parent1.id), 1, account_id_type(parent2.id), 1);
            make_child_op.active = authority(2, account_id_type(parent1.id), 1, account_id_type(parent2.id), 1);
            trx.operations.push_back(make_child_op);
            PUSH_TX( db.get(), trx, ~0);
            trx.operations.clear();
        }

        const account_object &child = get_account("child");
        auto old_balance = fund(child);

        BOOST_TEST_MESSAGE("Attempting to transfer with no signatures, should fail");
        transfer_operation op;
        op.from = child.id;
        op.amount = core.amount(500);
        trx.operations.push_back(op);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);

        BOOST_TEST_MESSAGE("Attempting to transfer with parent1 signature, should fail");
        sign(trx, parent1_key);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        trx.signatures.clear();

        BOOST_TEST_MESSAGE("Attempting to transfer with parent2 signature, should fail");
        sign(trx, parent2_key);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);

        BOOST_TEST_MESSAGE("Attempting to transfer with parent1 and parent2 signature, should succeed");
        sign(trx, parent1_key);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 500);
        trx.operations.clear();
        trx.signatures.clear();

        BOOST_TEST_MESSAGE("Adding a key for the child that can override parents");
        fc::ecc::private_key child_key = fc::ecc::private_key::generate();
        {
            account_update_operation op;
            op.account = child.id;
            op.active = authority(2, account_id_type(parent1.id), 1, account_id_type(parent2.id), 1,
                                  public_key_type(child_key.get_public_key()), 2);
            trx.operations.push_back(op);
            sign(trx, parent1_key);
            sign(trx, parent2_key);
            PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
            BOOST_REQUIRE_EQUAL(child.active.num_auths(), 3);
            trx.operations.clear();
            trx.signatures.clear();
        }

        op.from = child.id;
        op.to = account_id_type();
        op.amount = core.amount(500);
        trx.operations.push_back(op);

        BOOST_TEST_MESSAGE("Attempting transfer with no signatures, should fail");
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        BOOST_TEST_MESSAGE("Attempting transfer just parent1, should fail");
        sign(trx, parent1_key);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        trx.signatures.clear();
        BOOST_TEST_MESSAGE("Attempting transfer just parent2, should fail");
        sign(trx, parent2_key);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);

        BOOST_TEST_MESSAGE("Attempting transfer both parents, should succeed");
        sign(trx, parent1_key);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 1000);
        trx.signatures.clear();

        BOOST_TEST_MESSAGE("Attempting transfer with just child key, should succeed");
        sign(trx, child_key);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 1500);
        trx.operations.clear();
        trx.signatures.clear();

        BOOST_TEST_MESSAGE("Creating grandparent account, parent1 now requires authority of grandparent");
        auto grandparent = create_account("grandparent");
        fc::ecc::private_key grandparent_key = fc::ecc::private_key::generate();
        {
            account_update_operation op;
            op.account = parent1.id;
            op.active = authority(1, account_id_type(grandparent.id), 1);
            op.owner = *op.active;
            trx.operations.push_back(op);
            op.account = grandparent.id;
            op.active = authority(1, public_key_type(grandparent_key.get_public_key()), 1);
            op.owner = *op.active;
            trx.operations.push_back(op);
            PUSH_TX( db.get(), trx, ~0);
            trx.operations.clear();
            trx.signatures.clear();
        }

        BOOST_TEST_MESSAGE("Attempt to transfer using old parent keys, should fail");
        trx.operations.push_back(op);
        sign(trx, parent1_key);
        sign(trx, parent2_key);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        trx.signatures.clear();
        sign(trx, parent2_key);
        sign(trx, grandparent_key);

        BOOST_TEST_MESSAGE("Attempt to transfer using parent2_key and grandparent_key");
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 2000);
        trx.clear();

        BOOST_TEST_MESSAGE("Update grandparent account authority to be committee account");
        {
            account_update_operation op;
            op.account = grandparent.id;
            op.active = authority(1, account_id_type(), 1);
            op.owner = *op.active;
            trx.operations.push_back(op);
            PUSH_TX( db.get(), trx, ~0);
            trx.operations.clear();
            trx.signatures.clear();
        }

        BOOST_TEST_MESSAGE("Create recursion depth failure");
        trx.operations.push_back(op);
        sign(trx, parent2_key);
        sign(trx, grandparent_key);
        sign(trx, init_account_priv_key);
        //Fails due to recursion depth.
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
        BOOST_TEST_MESSAGE("verify child key can override recursion checks");
        trx.signatures.clear();
        sign(trx, child_key);
        PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check);
        BOOST_CHECK_EQUAL(get_balance(child, core), old_balance - 2500);
        trx.operations.clear();
        trx.signatures.clear();

        BOOST_TEST_MESSAGE("Verify a cycle fails");
        {
            account_update_operation op;
            op.account = parent1.id;
            op.active = authority(1, account_id_type(child.id), 1);
            op.owner = *op.active;
            trx.operations.push_back(op);
            PUSH_TX( db.get(), trx, ~0);
            trx.operations.clear();
            trx.signatures.clear();
        }

        trx.operations.push_back(op);
        sign(trx, parent2_key);
        //Fails due to recursion depth.
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx, database::skip_transaction_dupe_check), fc::exception);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_CASE(proposed_single_account)
{
    using namespace graphene::chain;
    try
    {
        INVOKE(any_two_of_three);

        fc::ecc::private_key committee_key = init_account_priv_key;
        fc::ecc::private_key nathan_key1 = fc::ecc::private_key::regenerate(fc::digest("key1"));
        fc::ecc::private_key nathan_key2 = fc::ecc::private_key::regenerate(fc::digest("key2"));
        fc::ecc::private_key nathan_key3 = fc::ecc::private_key::regenerate(fc::digest("key3"));

        const account_object &moneyman = create_account("moneyman", init_account_pub_key);
        const account_object &nathan = get_account("nathan");
        const asset_object &core = asset_id_type()(*db);

        transfer(account_id_type()(*db), moneyman, core.amount(1000000));

        //Following any_two_of_three, nathan's active authority is satisfied by any two of {key1,key2,key3}
        BOOST_TEST_MESSAGE("moneyman is creating proposal for nathan to transfer 100 CORE to moneyman");

        transfer_operation transfer_op;
        transfer_op.from = nathan.id;
        transfer_op.to = moneyman.get_id();
        transfer_op.amount = core.amount(100);

        proposal_create_operation op;
        op.fee_paying_account = moneyman.id;
        op.proposed_ops.emplace_back(transfer_op);
        op.expiration_time = db->head_block_time() + fc::days(3);

        asset nathan_start_balance = db->get_balance(nathan.get_id(), core.get_id());
        {
            vector<authority> other;
            flat_set<account_id_type> active_set, owner_set;
            operation_get_required_authorities(op, active_set, owner_set, other);
            BOOST_CHECK_EQUAL(active_set.size(), 1);
            BOOST_CHECK_EQUAL(owner_set.size(), 0);
            BOOST_CHECK_EQUAL(other.size(), 0);
            BOOST_CHECK(*active_set.begin() == moneyman.get_id());

            active_set.clear();
            other.clear();
            operation_get_required_authorities(op.proposed_ops.front().op, active_set, owner_set, other);
            BOOST_CHECK_EQUAL(active_set.size(), 1);
            BOOST_CHECK_EQUAL(owner_set.size(), 0);
            BOOST_CHECK_EQUAL(other.size(), 0);
            BOOST_CHECK(*active_set.begin() == nathan.id);
        }

        trx.operations.push_back(op);
        set_expiration( db.get(), trx);

        sign(trx, init_account_priv_key);
        const proposal_object &proposal = db->get<proposal_object>(PUSH_TX( db.get(), trx).operation_results.front().get<object_id_result>().result);

        BOOST_CHECK_EQUAL(proposal.required_active_approvals.size(), 1);
        BOOST_CHECK_EQUAL(proposal.available_active_approvals.size(), 0);
        BOOST_CHECK_EQUAL(proposal.required_owner_approvals.size(), 0);
        BOOST_CHECK_EQUAL(proposal.available_owner_approvals.size(), 0);
        BOOST_CHECK(*proposal.required_active_approvals.begin() == nathan.id);

        proposal_update_operation pup;
        pup.proposal = proposal.id;
        pup.fee_paying_account = nathan.id;
        BOOST_TEST_MESSAGE("Updating the proposal to have nathan's authority");
        pup.active_approvals_to_add.insert(nathan.id);

        trx.operations = {pup};
        sign(trx, committee_key);
        //committee may not add nathan's approval.
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx), fc::exception);
        pup.active_approvals_to_add.clear();
        pup.active_approvals_to_add.insert(account_id_type());
        trx.operations = {pup};
        sign(trx, committee_key);
        //committee has no stake in the transaction.
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx), fc::exception);

        trx.signatures.clear();
        pup.active_approvals_to_add.clear();
        pup.active_approvals_to_add.insert(nathan.id);

        trx.operations = {pup};
        sign(trx, nathan_key3);
        sign(trx, nathan_key2);

        BOOST_CHECK_EQUAL(get_balance(nathan, core), nathan_start_balance.amount.value);
        PUSH_TX( db.get(), trx);
        // fc::usleep(fc::milliseconds(6000));
        BOOST_CHECK_EQUAL(get_balance(nathan, core), nathan_start_balance.amount.value);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

/// Verify that committee authority cannot be invoked in a normal transaction
BOOST_AUTO_TEST_CASE(committee_authority)
{
    try
    {
        fc::ecc::private_key nathan_key = fc::ecc::private_key::generate();
        fc::ecc::private_key committee_key = init_account_priv_key;
        const account_object nathan = create_account("nathan", nathan_key.get_public_key());
        const auto &global_params = db->get_global_properties().parameters;

        generate_block();

        // Signatures are for suckers.
        db->modify(db->get_global_properties(), [](global_property_object &p) {
            // Turn the review period WAY down, so it doesn't take long to produce blocks to that point in simulated time.
            p.parameters.committee_proposal_review_period = 3600;
        });

        BOOST_TEST_MESSAGE("transfering 100000 CORE to nathan, signing with committee key should fail because this requires it to be part of a proposal");
        transfer_operation top;
        top.to = nathan.id;
        top.amount = asset(100000);
        trx.operations.push_back(top);
        sign(trx, committee_key);
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx), graphene::chain::invalid_committee_approval);

        auto _sign = [&] { trx.signatures.clear(); sign( trx, nathan_key ); };

        proposal_create_operation pop;
        pop.proposed_ops.push_back({trx.operations.front()});
        // pop.review_period_seconds = global_params.committee_proposal_review_period;
        pop.expiration_time = db->head_block_time() + global_params.committee_proposal_review_period * 2;
        pop.fee_paying_account = nathan.id;
        trx.operations = {pop};
        _sign();

        // The review period isn't set yet. Make sure it throws.
        // GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx), proposal_create_review_period_required);
        pop.review_period_seconds = global_params.committee_proposal_review_period / 2;
        trx.operations.back() = pop;
        _sign();
        // The review period is too short. Make sure it throws.
        // GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx), proposal_create_review_period_insufficient);
        pop.review_period_seconds = global_params.committee_proposal_review_period;
        trx.operations.back() = pop;
        _sign();
        proposal_object prop = db->get<proposal_object>(PUSH_TX( db.get(), trx).operation_results.front().get<object_id_result>().result);
        BOOST_REQUIRE(db->find_object(prop.id));

        BOOST_CHECK(prop.expiration_time == pop.expiration_time);
        BOOST_CHECK(prop.review_period_time && *prop.review_period_time == pop.expiration_time - *pop.review_period_seconds);
        BOOST_CHECK(prop.proposed_transaction.operations.size() == 1);
        BOOST_CHECK_EQUAL(get_balance(nathan, asset_id_type()(*db)), 0);
        BOOST_CHECK(!db->get<proposal_object>(prop.id).is_authorized_to_execute(*db));

        generate_block();
        BOOST_REQUIRE(db->find_object(prop.id));
        BOOST_CHECK_EQUAL(get_balance(nathan, asset_id_type()(*db)), 0);

        BOOST_TEST_MESSAGE("Checking that the proposal is not authorized to execute");
        BOOST_REQUIRE(!db->get<proposal_object>(prop.id).is_authorized_to_execute(*db));
        trx.operations.clear();
        trx.signatures.clear();
        proposal_update_operation uop;
        uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
        uop.proposal = prop.id;

        uop.key_approvals_to_add.emplace(committee_key.get_public_key());

        trx.operations.push_back(uop);
        sign(trx, committee_key);
        db->push_transaction(trx);
        BOOST_CHECK_EQUAL(get_balance(nathan, asset_id_type()(*db)), 0);
        BOOST_CHECK(db->get<proposal_object>(prop.id).is_authorized_to_execute(*db));

        trx.signatures.clear();
        generate_blocks(*prop.review_period_time);
        uop.key_approvals_to_add.clear();
        uop.key_approvals_to_add.insert(committee_key.get_public_key()); // was 7
        trx.operations.back() = uop;
        sign(trx, committee_key);
        // Should throw because the transaction is now in review.
        GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx), fc::exception);

        generate_blocks(prop.expiration_time);
        BOOST_CHECK_EQUAL(get_balance(nathan, asset_id_type()(*db)), 0);
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(fired_committee_members, database_fixture)
{
    try
    {
        generate_block();
        fc::ecc::private_key committee_key = init_account_priv_key;
        fc::ecc::private_key committee_member_key = fc::ecc::private_key::generate();

        //Meet nathan. He has a little money.
        const account_object *nathan = &create_account("nathan");
        transfer(account_id_type()(*db), *nathan, asset(5000));
        generate_block();
        nathan = &get_account("nathan");
        flat_set<vote_id_type> committee_members;

        /*
   db->modify(db->get_global_properties(), [](global_property_object& p) {
      // Turn the review period WAY down, so it doesn't take long to produce blocks to that point in simulated time.
      p.parameters.committee_proposal_review_period = fc::days(1).to_seconds();
   });
   */

        for (int i = 0; i < 15; ++i)
        {
            const auto &account = create_account("committee-member" + fc::to_string(i + 1), committee_member_key.get_public_key());
            upgrade_to_lifetime_member(account);
            committee_members.insert(create_committee_member(account).vote_id);
        }
        BOOST_REQUIRE_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);

        //A proposal is created to give nathan lots more money.
        auto params = db->get_global_properties().parameters;
        proposal_create_operation pop = proposal_create_operation::committee_proposal(params, db->head_block_time());
        pop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
        pop.expiration_time = db->head_block_time() + *pop.review_period_seconds + 1800;
        ilog("Creating proposal to give nathan money that expires: ${e}", ("e", pop.expiration_time));
        ilog("The proposal has a review period of: ${r} sec", ("r", *pop.review_period_seconds));

        transfer_operation top;
        top.to = nathan->id;
        top.amount = asset(100000);
        pop.proposed_ops.emplace_back(top);
        trx.operations.push_back(pop);
        const proposal_object &prop = db->get<proposal_object>(PUSH_TX( db.get(), trx).operation_results.front().get<object_id_result>().result);
        proposal_id_type pid = prop.id;
        BOOST_CHECK(!pid(*db).is_authorized_to_execute(*db));

        ilog("commitee member approves proposal");
        //committee key approves of the proposal.
        proposal_update_operation uop;
        uop.fee_paying_account = GRAPHENE_TEMP_ACCOUNT;
        uop.proposal = pid;
        uop.key_approvals_to_add.emplace(init_account_pub_key);
        trx.operations.back() = uop;
        sign(trx, committee_key);
        PUSH_TX( db.get(), trx);
        BOOST_CHECK(pid(*db).is_authorized_to_execute(*db));

        ilog("Generating blocks for 2 days");
        generate_block();
        BOOST_REQUIRE_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);
        generate_block();
        BOOST_REQUIRE_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);
        //Time passes... the proposal is now in its review period.
        //generate_blocks(*pid(*db).review_period_time);
        generate_blocks(db->head_block_time() + fc::days(1));
        ilog("head block time: ${t}", ("t", db->head_block_time()));

        fc::time_point_sec maintenance_time = db->get_dynamic_global_properties().next_maintenance_time;
        BOOST_CHECK_LT(maintenance_time.sec_since_epoch(), pid(*db).expiration_time.sec_since_epoch());
        //Yay! The proposal to give nathan more money is authorized.
        BOOST_REQUIRE(pid(*db).is_authorized_to_execute(*db));

        nathan = &get_account("nathan");
        // no money yet
        BOOST_REQUIRE_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);

        {
            //Oh noes! Nathan votes for a whole new slate of committee_members!
            account_update_operation op;
            op.account = nathan->id;
            op.new_options = nathan->options;
            op.new_options->votes = committee_members;
            trx.operations.push_back(op);
            set_expiration( db.get(), trx);
            PUSH_TX( db.get(), trx, ~0);
            trx.operations.clear();
        }
        idump((get_balance(*nathan, asset_id_type()(*db))));
        // still no money
        BOOST_CHECK_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);

        //Time passes... the set of active committee_members gets updated.
        generate_blocks(maintenance_time);
        //The proposal is no longer authorized, because the active committee_members got changed.
        BOOST_CHECK(!pid(*db).is_authorized_to_execute(*db));
        // still no money
        BOOST_CHECK_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);

        //Time passes... the proposal has now expired.
        generate_blocks(pid(*db).expiration_time);
        BOOST_CHECK(db->find(pid) == nullptr);

        //Nathan never got any more money because the proposal was rejected.
        BOOST_CHECK_EQUAL(get_balance(*nathan, asset_id_type()(*db)), 5000);
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(proposal_two_accounts, database_fixture)
{
    try
    {
        generate_block();

        auto nathan_key = generate_private_key("nathan");
        auto dingss_key = generate_private_key("dingss");
        const account_object &nathan = create_account("nathan", nathan_key.get_public_key());
        const account_object &dingss = create_account("dingss", dingss_key.get_public_key());

        transfer(account_id_type()(*db), nathan, asset(100000));
        transfer(account_id_type()(*db), dingss, asset(100000));

        {
            transfer_operation top;
            top.from = dingss.get_id();
            top.to = nathan.get_id();
            top.amount = asset(500);

            proposal_create_operation pop;
            pop.proposed_ops.emplace_back(top);
            std::swap(top.from, top.to);
            pop.proposed_ops.emplace_back(top);

            pop.fee_paying_account = nathan.get_id();
            pop.expiration_time = db->head_block_time() + fc::days(1);
            trx.operations.push_back(pop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
        }

        const proposal_object &prop = *db->get_index_type<proposal_index>().indices().begin();
        BOOST_CHECK(prop.required_active_approvals.size() == 2);
        BOOST_CHECK(prop.required_owner_approvals.size() == 0);
        BOOST_CHECK(!prop.is_authorized_to_execute(*db));

        {
            proposal_id_type pid = prop.id;
            proposal_update_operation uop;
            uop.proposal = prop.id;
            uop.active_approvals_to_add.insert(nathan.get_id());
            uop.fee_paying_account = nathan.get_id();
            trx.operations.push_back(uop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();

            BOOST_CHECK(db->find_object(pid) != nullptr);
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));

            uop.active_approvals_to_add = {dingss.get_id()};
            trx.operations.push_back(uop);
            sign(trx, nathan_key);
            GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx), fc::exception);
            sign(trx, dingss_key);
            PUSH_TX( db.get(), trx);

            BOOST_CHECK(db->find_object(pid) != nullptr);
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(proposal_delete, database_fixture)
{
    try
    {
        generate_block();

        auto nathan_key = generate_private_key("nathan");
        auto dingss_key = generate_private_key("dingss");
        const account_object &nathan = create_account("nathan", nathan_key.get_public_key());
        const account_object &dingss = create_account("dingss", dingss_key.get_public_key());

        transfer(account_id_type()(*db), nathan, asset(100000));
        transfer(account_id_type()(*db), dingss, asset(100000));

        {
            transfer_operation top;
            top.from = dingss.get_id();
            top.to = nathan.get_id();
            top.amount = asset(500);

            proposal_create_operation pop;
            pop.proposed_ops.emplace_back(top);
            std::swap(top.from, top.to);
            top.amount = asset(6000);
            pop.proposed_ops.emplace_back(top);

            pop.fee_paying_account = nathan.get_id();
            pop.expiration_time = db->head_block_time() + fc::days(1);
            trx.operations.push_back(pop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
        }

        const proposal_object &prop = *db->get_index_type<proposal_index>().indices().begin();
        BOOST_CHECK(prop.required_active_approvals.size() == 2);
        BOOST_CHECK(prop.required_owner_approvals.size() == 0);
        BOOST_CHECK(!prop.is_authorized_to_execute(*db));

        {
            proposal_update_operation uop;
            uop.fee_paying_account = nathan.get_id();
            uop.proposal = prop.id;
            uop.active_approvals_to_add.insert(nathan.get_id());
            trx.operations.push_back(uop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_active_approvals.size(), 1);

            std::swap(uop.active_approvals_to_add, uop.active_approvals_to_remove);
            trx.operations.push_back(uop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_active_approvals.size(), 0);
        }

        {
            proposal_id_type pid = prop.id;
            proposal_delete_operation dop;
            dop.fee_paying_account = nathan.get_id();
            dop.proposal = pid;
            trx.operations.push_back(dop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            BOOST_CHECK(db->find_object(pid) == nullptr);
            BOOST_CHECK_EQUAL(get_balance(nathan, asset_id_type()(*db)), 100000);
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(proposal_owner_authority_delete, database_fixture)
{
    try
    {
        generate_block();

        auto nathan_key = generate_private_key("nathan");
        auto dingss_key = generate_private_key("dingss");
        const account_object &nathan = create_account("nathan", nathan_key.get_public_key());
        const account_object &dingss = create_account("dingss", dingss_key.get_public_key());

        transfer(account_id_type()(*db), nathan, asset(100000));
        transfer(account_id_type()(*db), dingss, asset(100000));

        {
            transfer_operation top;
            top.from = dingss.get_id();
            top.to = nathan.get_id();
            top.amount = asset(500);

            account_update_operation uop;
            uop.account = nathan.get_id();
            uop.owner = authority(1, public_key_type(generate_private_key("nathan2").get_public_key()), 1);

            proposal_create_operation pop;
            pop.proposed_ops.emplace_back(top);
            pop.proposed_ops.emplace_back(uop);
            std::swap(top.from, top.to);
            top.amount = asset(6000);
            pop.proposed_ops.emplace_back(top);

            pop.fee_paying_account = nathan.get_id();
            pop.expiration_time = db->head_block_time() + fc::days(1);
            trx.operations.push_back(pop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
        }

        const proposal_object &prop = *db->get_index_type<proposal_index>().indices().begin();
        BOOST_CHECK_EQUAL(prop.required_active_approvals.size(), 1);
        BOOST_CHECK_EQUAL(prop.required_owner_approvals.size(), 1);
        BOOST_CHECK(!prop.is_authorized_to_execute(*db));

        {
            proposal_update_operation uop;
            uop.fee_paying_account = nathan.get_id();
            uop.proposal = prop.id;
            uop.owner_approvals_to_add.insert(nathan.get_id());
            trx.operations.push_back(uop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_owner_approvals.size(), 1);

            std::swap(uop.owner_approvals_to_add, uop.owner_approvals_to_remove);
            trx.operations.push_back(uop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_owner_approvals.size(), 0);
        }

        {
            proposal_id_type pid = prop.id;
            proposal_delete_operation dop;
            dop.fee_paying_account = nathan.get_id();
            dop.proposal = pid;
            dop.using_owner_authority = true;
            trx.operations.push_back(dop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            BOOST_CHECK(db->find_object(pid) == nullptr);
            BOOST_CHECK_EQUAL(get_balance(nathan, asset_id_type()(*db)), 100000);
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(proposal_owner_authority_complete, database_fixture)
{
    try
    {
        generate_block();

        auto nathan_key = generate_private_key("nathan");
        auto dingss_key = generate_private_key("dingss");
        const account_object &nathan = create_account("nathan", nathan_key.get_public_key());
        const account_object &dingss = create_account("dingss", dingss_key.get_public_key());

        transfer(account_id_type()(*db), nathan, asset(100000));
        transfer(account_id_type()(*db), dingss, asset(100000));

        {
            transfer_operation top;
            top.from = dingss.get_id();
            top.to = nathan.get_id();
            top.amount = asset(500);

            account_update_operation uop;
            uop.account = nathan.get_id();
            uop.owner = authority(1, public_key_type(generate_private_key("nathan2").get_public_key()), 1);

            proposal_create_operation pop;
            pop.proposed_ops.emplace_back(top);
            pop.proposed_ops.emplace_back(uop);
            std::swap(top.from, top.to);
            top.amount = asset(6000);
            pop.proposed_ops.emplace_back(top);

            pop.fee_paying_account = nathan.get_id();
            pop.expiration_time = db->head_block_time() + fc::days(1);
            trx.operations.push_back(pop);
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
        }

        const proposal_object &prop = *db->get_index_type<proposal_index>().indices().begin();
        BOOST_CHECK_EQUAL(prop.required_active_approvals.size(), 1);
        BOOST_CHECK_EQUAL(prop.required_owner_approvals.size(), 1);
        BOOST_CHECK(!prop.is_authorized_to_execute(*db));

        {
            proposal_id_type pid = prop.id;
            proposal_update_operation uop;
            uop.fee_paying_account = nathan.get_id();
            uop.proposal = prop.id;
            uop.key_approvals_to_add.insert(dingss.active.key_auths.begin()->first);
            trx.operations.push_back(uop);
            set_expiration( db.get(), trx);
            sign(trx, nathan_key);
            sign(trx, dingss_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_key_approvals.size(), 1);

            std::swap(uop.key_approvals_to_add, uop.key_approvals_to_remove);
            trx.operations.push_back(uop);
            trx.expiration += fc::seconds(1); // Survive trx dupe check
            sign(trx, nathan_key);
            sign(trx, dingss_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_key_approvals.size(), 0);

            std::swap(uop.key_approvals_to_add, uop.key_approvals_to_remove);
            trx.operations.push_back(uop);
            trx.expiration += fc::seconds(1); // Survive trx dupe check
            sign(trx, nathan_key);
            sign(trx, dingss_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(!prop.is_authorized_to_execute(*db));
            BOOST_CHECK_EQUAL(prop.available_key_approvals.size(), 1);

            uop.key_approvals_to_add.clear();
            uop.owner_approvals_to_add.insert(nathan.get_id());
            trx.operations.push_back(uop);
            trx.expiration += fc::seconds(1); // Survive trx dupe check
            sign(trx, nathan_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
            BOOST_CHECK(db->find_object(pid) != nullptr);
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(max_authority_membership, database_fixture)
{
    try
    {
        //Get a sane head block time
        generate_block();

        db->modify(db->get_global_properties(), [](global_property_object &p) {
            p.parameters.committee_proposal_review_period = fc::hours(1).to_seconds();
        });

        transaction tx;
        processed_transaction ptx;

        private_key_type committee_key = init_account_priv_key;
        // Samtest is the creator of accounts
        private_key_type sam_key = generate_private_key("samtest");
        account_object sam_account_object = create_account("samtest", sam_key);
        upgrade_to_lifetime_member(sam_account_object);
        account_object committee_account_object = committee_account(*db);

        const asset_object &core = asset_id_type()(*db);

        transfer(committee_account_object, sam_account_object, core.amount(100000));
        wdump((get_balance(sam_account_object, asset_id_type()(*db))));

        // have Sam create some keys

        int keys_to_create = GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP;
        vector<private_key_type> private_keys;

        private_keys.reserve(keys_to_create);
        for (int i = 0; i < keys_to_create; i++)
        {
            string seed = "this_is_a_key_" + std::to_string(i);
            private_key_type privkey = generate_private_key(seed);
            private_keys.push_back(privkey);
        }
        set_expiration( db.get(), tx);

        vector<public_key_type> key_ids;

        key_ids.reserve(keys_to_create);
        for (int i = 0; i < keys_to_create; i++)
            key_ids.push_back(private_keys[i].get_public_key());

        // now try registering accounts with n keys, 0 < n < 20

        // TODO:  Make sure it throws / accepts properly when
        //  max_account_authority is changed in global parameteres

        for (int num_keys = 1; num_keys <= keys_to_create; num_keys++)
        {
            // try registering account with n keys

            authority test_authority;
            test_authority.weight_threshold = num_keys;

            for (int i = 0; i < num_keys; i++)
                test_authority.key_auths[key_ids[i]] = 1;

            auto check_tx = [&](const authority &owner_auth,
                                const authority &active_auth) {
                const uint16_t max_authority_membership = GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP;
                account_create_operation anon_create_op;
                transaction tx;

                anon_create_op.owner = owner_auth;
                anon_create_op.active = active_auth;
                anon_create_op.registrar = sam_account_object.id;
                anon_create_op.options.memo_key = sam_account_object.options.memo_key;
                anon_create_op.name = generate_anon_acct_name();

                tx.operations.push_back(anon_create_op);
                set_expiration( db.get(), tx);

                if (num_keys > max_authority_membership)
                {
                    wdump((num_keys)(max_authority_membership));
                    GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), tx, ~0), account_create_max_auth_exceeded);
                }
                else
                {
                    PUSH_TX( db.get(), tx, ~0);
                }
                return;
            };

            check_tx(sam_account_object.owner, test_authority);
            check_tx(test_authority, sam_account_object.active);
            check_tx(test_authority, test_authority);
        }
    }
    FC_LOG_AND_RETHROW()
}

BOOST_FIXTURE_TEST_CASE(bogus_signature, database_fixture)
{
    try
    {
        private_key_type committee_key = init_account_priv_key;
        // Sam is the creator of accounts
        private_key_type alice_key = generate_private_key("alicetest");
        private_key_type antilope_key = generate_private_key("antilope");
        private_key_type charlie_key = generate_private_key("charlietest");

        account_object committee_account_object = committee_account(*db);
        account_object alice_account_object = create_account("alicetest", alice_key);
        account_object antilope_account_object = create_account("antilope", antilope_key);
        account_object charlie_account_object = create_account("charlietest", charlie_key);

        // unneeded, comment it out to silence compiler warning
        //key_id_type antilope_key_id = antilope_account_object.memo_key;

        uint32_t skip = database::skip_transaction_dupe_check;

        // send from Sam -> Alice, signed by Sam

        const asset_object &core = asset_id_type()(*db);
        transfer(committee_account_object, alice_account_object, core.amount(100000));
        wdump((get_balance(alice_account_object, asset_id_type()(*db))));

        transfer_operation xfer_op;
        xfer_op.from = alice_account_object.id;
        xfer_op.to = antilope_account_object.id;
        xfer_op.amount = core.amount(5000);
        // xfer_op.fee = db->current_fee_schedule().calculate_fee(xfer_op);

        trx.clear();
        trx.operations.push_back(xfer_op);

        BOOST_TEST_MESSAGE("Transfer signed by alice");
        sign(trx, alice_key);

        flat_set<account_id_type> active_set, owner_set;
        vector<authority> others;
        trx.get_required_authorities(active_set, owner_set, others);

        PUSH_TX( db.get(), trx, skip);

        trx.operations.push_back(xfer_op);
        BOOST_TEST_MESSAGE("Invalidating Alices Signature");
        // Alice's signature is now invalid
        GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx, skip), fc::exception);
        // Re-sign, now OK (sig is replaced)
        BOOST_TEST_MESSAGE("Resign with Alice's Signature");
        trx.signatures.clear();
        sign(trx, alice_key);
        PUSH_TX( db.get(), trx, skip);

        trx.signatures.clear();
        trx.operations.pop_back();
        sign(trx, alice_key);
        sign(trx, charlie_key);
        // Signed by third-party Charlie (irrelevant key, not in authority)
        // GRAPHENE_REQUIRE_THROW(PUSH_TX( db.get(), trx, skip), tx_irrelevant_sig);
    }
    FC_LOG_AND_RETHROW()
}
/*
BOOST_FIXTURE_TEST_CASE(voting_account, database_fixture)
{
    try
    {
        ACTORS((nathan)(vikram));
        upgrade_to_lifetime_member(nathan_id);
        upgrade_to_lifetime_member(vikram_id);
        committee_member_id_type nathan_committee_member = create_committee_member(nathan_id(*db)).id;
        committee_member_id_type vikram_committee_member = create_committee_member(vikram_id(*db)).id;

        //wdump((db->get_balance(account_id_type(), asset_id_type())));
        generate_block();

        wdump((db->get_balance(account_id_type(), asset_id_type())));
        transfer(account_id_type(), nathan_id, asset(1000000));
        transfer(account_id_type(), vikram_id, asset(100));
        wdump((db->get_balance(nathan_id, asset_id_type())));
        wdump((db->get_balance(vikram_id, asset_id_type())));

        {
            account_update_operation op;
            op.account = nathan_id;
            op.new_options = nathan_id(*db).options;
            // op.new_options->voting_account = vikram_id;
            op.new_options->votes = flat_set<vote_id_type>{nathan_committee_member(*db).vote_id};
            // op.new_options->num_committee = 1;
            trx.operations.push_back(op);
            sign(trx, nathan_private_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
        }
        {
            account_update_operation op;
            op.account = vikram_id;
            op.new_options = vikram_id(*db).options;
            op.new_options->votes.insert(vikram_committee_member(*db).vote_id);
            // op.new_options->num_committee = 11;
            trx.operations.push_back(op);
            sign(trx, vikram_private_key);
            // Fails because num_committee is larger than the cardinality of committee members being voted for
            GRAPHENE_CHECK_THROW(PUSH_TX( db.get(), trx), fc::exception);
            // op.new_options->num_committee = 3;
            trx.operations = {op};
            trx.signatures.clear();
            sign(trx, vikram_private_key);
            PUSH_TX( db.get(), trx);
            trx.clear();
        }

        generate_blocks(db->get_dynamic_global_properties().next_maintenance_time + GRAPHENE_DEFAULT_BLOCK_INTERVAL);
        BOOST_CHECK(std::find(db->get_global_properties().active_committee_members.begin(),
                              db->get_global_properties().active_committee_members.end(),
                              nathan_committee_member) == db->get_global_properties().active_committee_members.end());
        BOOST_CHECK(std::find(db->get_global_properties().active_committee_members.begin(),
                              db->get_global_properties().active_committee_members.end(),
                              vikram_committee_member) != db->get_global_properties().active_committee_members.end());
    }
    FC_LOG_AND_RETHROW()
}
 */

/*
 * Simple corporate accounts:
 *
 * wells Corp.       Alice 50, antilope 50             T=60
 * xylos Company     Alice 30, Cindy 50           T=40
 * yayas Inc.        antilope 10, dingss 10, edyes 10       T=20
 * zyzys Co.         dingss 50                       T=40
 *
 * Complex corporate accounts:
 *
 * megas Corp.       wells 30, Yes 30              T=40
 * novas Ltd.        Alice 10, wells 10            T=20
 * odles Intl.       dingss 10, Yes 10, zyzys 10      T=20
 * poxxs LLC         wells 10, xylos 10, Yes 20, zyzys 20   T=40
 */

BOOST_FIXTURE_TEST_CASE(get_required_signatures_test, database_fixture)
{
    try
    {
        ACTORS(
            (alice)(antilope)(cindy)(dingss)(edyes)(megas)(novas)(odles)(poxxs)(wells)(xylos)(yayas)(zyzys));

        auto set_auth = [&](
                            account_id_type aid,
                            const authority &auth) {
            signed_transaction tx;
            account_update_operation op;
            op.account = aid;
            op.active = auth;
            op.owner = auth;
            tx.operations.push_back(op);
            set_expiration( db.get(), tx);
            PUSH_TX( db.get(), tx, database::skip_transaction_signatures | database::skip_authority_check);
        };

        auto get_active = [&](
                              account_id_type aid) -> const authority * {
            return &(aid(*db).active);
        };

        auto get_owner = [&](
                             account_id_type aid) -> const authority * {
            return &(aid(*db).owner);
        };

        auto chk = [&](
                       const signed_transaction &tx,
                       flat_set<public_key_type> available_keys,
                       set<public_key_type> ref_set) -> bool {
            //wdump( (tx)(available_keys) );
            set<public_key_type> result_set = tx.get_required_signatures(db->get_chain_id(), available_keys, get_active, get_owner);
            //wdump( (result_set)(ref_set) );
            return result_set == ref_set;
        };

        set_auth(wells_id, authority(60, alice_id, 50, antilope_id, 50));
        set_auth(xylos_id, authority(40, alice_id, 30, cindy_id, 50));
        set_auth(yayas_id, authority(20, antilope_id, 10, dingss_id, 10, edyes_id, 10));
        set_auth(zyzys_id, authority(40, dingss_id, 50));

        set_auth(megas_id, authority(40, wells_id, 30, yayas_id, 30));
        set_auth(novas_id, authority(20, alice_id, 10, wells_id, 10));
        set_auth(odles_id, authority(20, dingss_id, 10, yayas_id, 10, zyzys_id, 10));
        set_auth(poxxs_id, authority(40, wells_id, 10, xylos_id, 10, yayas_id, 20, zyzys_id, 20));

        signed_transaction tx;
        flat_set<public_key_type> all_keys{alice_public_key, antilope_public_key, cindy_public_key, dingss_public_key, edyes_public_key};

        tx.operations.push_back(transfer_operation());
        transfer_operation &op = tx.operations.back().get<transfer_operation>();
        op.to = edyes_id;
        op.amount = asset(1);

        op.from = alice_id;
        BOOST_CHECK(chk(tx, all_keys, {alice_public_key}));
        op.from = antilope_id;
        BOOST_CHECK(chk(tx, all_keys, {antilope_public_key}));
        op.from = wells_id;
        BOOST_CHECK(chk(tx, all_keys, {alice_public_key, antilope_public_key}));
        op.from = xylos_id;
        BOOST_CHECK(chk(tx, all_keys, {alice_public_key, cindy_public_key}));
        op.from = yayas_id;
        BOOST_CHECK(chk(tx, all_keys, {antilope_public_key, dingss_public_key}));
        op.from = zyzys_id;
        BOOST_CHECK(chk(tx, all_keys, {dingss_public_key}));

        op.from = megas_id;
        BOOST_CHECK(chk(tx, all_keys, {alice_public_key, antilope_public_key, dingss_public_key}));
        op.from = novas_id;
        BOOST_CHECK(chk(tx, all_keys, {alice_public_key, antilope_public_key}));
        op.from = odles_id;
        BOOST_CHECK(chk(tx, all_keys, {antilope_public_key, dingss_public_key}));
        op.from = poxxs_id;
        BOOST_CHECK(chk(tx, all_keys, {alice_public_key, antilope_public_key, cindy_public_key, dingss_public_key}));

        // TODO:  Add sigs to tx, then check
        // TODO:  Check removing sigs
        // TODO:  Accounts with mix of keys and accounts in their authority
        // TODO:  Tx with multiple ops requiring different sigs
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

/*
 * Pathological case
 *
 *      rocos(T=2)
 *    1/         \2
 *   styxs(T=2)   thuds(T=1)
 *  1/     \1       |1
 * Alice  antilope     Alice
 */

BOOST_FIXTURE_TEST_CASE(nonminimal_sig_test, database_fixture)
{
    try
    {
        ACTORS(
            (alice)(antilope)(rocos)(styxs)(thuds));

        auto set_auth = [&](
                            account_id_type aid,
                            const authority &auth) {
            signed_transaction tx;
            account_update_operation op;
            op.account = aid;
            op.active = auth;
            op.owner = auth;
            tx.operations.push_back(op);
            set_expiration( db.get(), tx);
            PUSH_TX( db.get(), tx, database::skip_transaction_signatures | database::skip_authority_check);
        };

        auto get_active = [&](
                              account_id_type aid) -> const authority * {
            return &(aid(*db).active);
        };

        auto get_owner = [&](
                             account_id_type aid) -> const authority * {
            return &(aid(*db).owner);
        };

        auto chk = [&](
                       const signed_transaction &tx,
                       flat_set<public_key_type> available_keys,
                       set<public_key_type> ref_set) -> bool {
            //wdump( (tx)(available_keys) );
            set<public_key_type> result_set = tx.get_required_signatures(db->get_chain_id(), available_keys, get_active, get_owner);
            //wdump( (result_set)(ref_set) );
            return result_set == ref_set;
        };

        auto chk_min = [&](
                           const signed_transaction &tx,
                           flat_set<public_key_type> available_keys,
                           set<public_key_type> ref_set) -> bool {
            //wdump( (tx)(available_keys) );
            set<public_key_type> result_set = tx.minimize_required_signatures(db->get_chain_id(), available_keys, get_active, get_owner);
            //wdump( (result_set)(ref_set) );
            return result_set == ref_set;
        };

        set_auth(rocos_id, authority(2, styxs_id, 1, thuds_id, 2));
        set_auth(styxs_id, authority(2, alice_id, 1, antilope_id, 1));
        set_auth(thuds_id, authority(1, alice_id, 1));

        signed_transaction tx;
        transfer_operation op;
        op.from = rocos_id;
        op.to = antilope_id;
        op.amount = asset(1);
        tx.operations.push_back(op);

        BOOST_CHECK(chk(tx, {alice_public_key, antilope_public_key}, {alice_public_key, antilope_public_key}));
        BOOST_CHECK(chk_min(tx, {alice_public_key, antilope_public_key}, {alice_public_key}));
         flat_set<public_key_type> sigkeys;
        GRAPHENE_REQUIRE_THROW(tx.verify_authority(db->get_chain_id(), get_active, get_owner,sigkeys), fc::exception);
        sign(tx, alice_private_key);
        tx.verify_authority(db->get_chain_id(), get_active, get_owner, sigkeys);
    }
    catch (fc::exception &e)
    {
        edump((e.to_detail_string()));
        throw;
    }
}

BOOST_AUTO_TEST_SUITE_END()
