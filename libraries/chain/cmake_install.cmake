# Install script for directory: /home/nico/walker/libraries/chain

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/nico/walker/libraries/chain/libgraphene_chain.a")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/graphene/chain" TYPE FILE FILES
    "/home/nico/walker/libraries/chain/include/graphene/chain/OperateItem.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/account_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/account_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/assert_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/asset_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/asset_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/balance_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/balance_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/block_database.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/block_summary_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/budget_record_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/buyback.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/buyback_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/chain_property_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/committee_member_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/committee_member_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/confidential_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/confidential_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/config.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/contract_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/contract_function_register_scheduler.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/contract_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/contract_session.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/crontab_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/crontab_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/custom_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/database.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/db_notify.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/db_with.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/exceptions.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/fba_accumulator_id.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/fba_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/file_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/file_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/fork_database.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/genesis_state.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/get_config.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/global_property_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/hardfork.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/immutable_chain_parameters.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/internal_exceptions.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/is_authorized_asset.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/lua_scheduler.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/market_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/market_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/nh_asset_creator_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/nh_asset_creator_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/nh_asset_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/nh_asset_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/nh_asset_order_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/nh_asset_order_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/node_property_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/operation_history_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/proposal_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/proposal_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/pts_address.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/special_authority.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/special_authority_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/temporary_authority.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/temporary_authority_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/transaction_evaluation_state.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/transaction_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/transfer_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/vesting_balance_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/vesting_balance_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/vote_count.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/withdraw_permission_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/withdraw_permission_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/witness_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/witness_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/witness_schedule_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/worker_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/worker_object.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/world_view_evaluator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/world_view_object.hpp"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/graphene/chain/protocol" TYPE FILE FILES
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/account.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/address.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/assert.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/asset.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/asset_ops.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/authority.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/balance.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/base.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/block.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/buyback.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/chain_parameters.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/committee_member.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/confidential.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/config.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/contract.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/crontab.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/custom.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/ext.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/fba.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/fee_schedule.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/file.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/lua_types.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/market.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/memo.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/nh_asset.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/nh_asset_creator.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/nh_asset_order.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/operations.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/proposal.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/protocol.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/special_authority.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/temporary.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/transaction.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/transfer.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/types.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/vesting.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/vote.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/withdraw_permission.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/witness.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/worker.hpp"
    "/home/nico/walker/libraries/chain/include/graphene/chain/protocol/world_view.hpp"
    )
endif()

