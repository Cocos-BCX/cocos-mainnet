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

#define GRAPHENE_SYMBOL "COCOS"
#define GRAPHENE_ADDRESS_PREFIX "COCOS"
#define CONTRACT_PROCESS_CIPHER "COCOS"

#define GRAPHENE_MIN_ACCOUNT_NAME_LENGTH 5
#define GRAPHENE_MAX_ACCOUNT_NAME_LENGTH 63

#define GRAPHENE_MIN_ASSET_SYMBOL_LENGTH 3
#define GRAPHENE_MAX_ASSET_SYMBOL_LENGTH 16

#define GRAPHENE_MAX_SHARE_SUPPLY int64_t(1000000000000000000ll)
#define GRAPHENE_MAX_PAY_RATE 10000 /* 100% */
#define GRAPHENE_MAX_SIG_CHECK_DEPTH 2
/**
 * Don't allow the committee_members to publish a limit that would
 * make the network unable to operate.
 */
#define GRAPHENE_MIN_TRANSACTION_SIZE_LIMIT 1024
#define GRAPHENE_MIN_BLOCK_INTERVAL   1 /* seconds */
#define GRAPHENE_MAX_BLOCK_INTERVAL  30 /* seconds */

#define GRAPHENE_DEFAULT_BLOCK_INTERVAL  5 /* seconds */
//#define GRAPHENE_DEFAULT_MAX_TRANSACTION_SIZE 2048
#define GRAPHENE_DEFAULT_MAX_BLOCK_SIZE  (2*1000*1000) /* < 2 MiB (less than MAX_MESSAGE_SIZE in graphene/net/config.hpp) */
#define GRAPHENE_DEFAULT_MAX_TIME_UNTIL_EXPIRATION (60*60*24) // seconds,  aka: 1 day
#define GRAPHENE_DEFAULT_MAINTENANCE_INTERVAL  (60*60*24) // seconds, aka: 1 day
#define GRAPHENE_DEFAULT_MAINTENANCE_SKIP_SLOTS 3  // number of slots to skip for maintenance interval

#define GRAPHENE_MIN_UNDO_HISTORY 10
#define GRAPHENE_MAX_UNDO_HISTORY 10000

#define GRAPHENE_MIN_BLOCK_SIZE_LIMIT (GRAPHENE_MIN_TRANSACTION_SIZE_LIMIT*5) // 5 transactions per block
#define GRAPHENE_MIN_TRANSACTION_EXPIRATION_LIMIT (GRAPHENE_MAX_BLOCK_INTERVAL * 5) // 5 transactions per block
#define GRAPHENE_BLOCKCHAIN_PRECISION                           uint64_t( 100000 )

#define GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS                    5 // 核心资产精度
#define GRAPHENE_DEFAULT_TRANSFER_FEE                           (1*GRAPHENE_BLOCKCHAIN_PRECISION)
#define GRAPHENE_MAX_INSTANCE_ID                                (uint64_t(-1)>>16)
/** percentage fields are fixed point with a denominator of 10,000 */
#define GRAPHENE_100_PERCENT                                    10000
#define GRAPHENE_1_PERCENT                                      (GRAPHENE_100_PERCENT/100)
#define GRAPHENE_51_PERCENT                                     (51*GRAPHENE_1_PERCENT)
/** NOTE: making this a power of 2 (say 2^15) would greatly accelerate fee calcs */
#define GRAPHENE_MAX_MARKET_FEE_PERCENT                         GRAPHENE_100_PERCENT
#define GRAPHENE_DEFAULT_FORCE_SETTLEMENT_DELAY                 (60*60*24) ///< 1 day
#define GRAPHENE_DEFAULT_FORCE_SETTLEMENT_OFFSET                0 ///< 1%
#define GRAPHENE_DEFAULT_FORCE_SETTLEMENT_MAX_VOLUME            (20* GRAPHENE_1_PERCENT) ///< 20%
#define GRAPHENE_DEFAULT_PRICE_FEED_LIFETIME                    (60*60*24) ///< 1 day
#define GRAPHENE_MAX_FEED_PRODUCERS                             200
#define GRAPHENE_DEFAULT_MAX_AUTHORITY_MEMBERSHIP               10
//#define GRAPHENE_DEFAULT_MAX_ASSET_WHITELIST_AUTHORITIES        10
#define GRAPHENE_DEFAULT_MAX_ASSET_FEED_PUBLISHERS              10

/**
 *  These ratios are fixed point numbers with a denominator of GRAPHENE_COLLATERAL_RATIO_DENOM, the
 *  minimum maitenance collateral is therefore 1.001x and the default
 *  maintenance ratio is 1.75x
 */
///@{
#define GRAPHENE_COLLATERAL_RATIO_DENOM                 1000
#define GRAPHENE_MIN_COLLATERAL_RATIO                   1001  ///< lower than this could result in divide by 0
#define GRAPHENE_MAX_COLLATERAL_RATIO                   32000 ///< higher than this is unnecessary and may exceed int16 storage
#define GRAPHENE_DEFAULT_MAINTENANCE_COLLATERAL_RATIO   1750 ///< Call when collateral only pays off 175% the debt
#define GRAPHENE_DEFAULT_MAX_SHORT_SQUEEZE_RATIO        1500 ///< Stop calling when collateral only pays off 150% of the debt
#define MAXIMUN_RUN_TIME_RATIO                          5000 ///nico add:: 合约最大占空比::5000/10000
#define MAXIMUN_NH_ASSET_ORDER_EXPIRATION               (60*60*24*7*2) // zhangfan add :: (Two weeks) maximum expiration time of nht order
#define MAXIMUN_HANDLING_FEE                            100*std::pow(10,GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS)
#define ASSIGNED_TASK_LIFE_CYCLE                        300  /// nico add ::约定任务生命周期　单位（秒)
#define CRONTAB_SUSPEND_THRESHOLD                       3 // zf add:: 定时任务挂起阈值，连续失败3次后任务挂起
#define CRONTAB_SUSPEND_EXPIRATION                      2592000  // zf add:: 定时任务被挂起后的过期时长
#define GRAPHENE_DEFAULT_MARGIN_PERIOD_SEC              (30*60*60*24)

#define GRAPHENE_DEFAULT_MIN_WITNESS_COUNT                    (11)
#define GRAPHENE_DEFAULT_MIN_COMMITTEE_MEMBER_COUNT           (11)
#define GRAPHENE_DEFAULT_WITNESSE_NUMBER                        (11) // SHOULD BE ODD
#define GRAPHENE_DEFAULT_COMMITTEE_NUMBER                        (11) // SHOULD BE ODD
#define GRAPHENE_DEFAULT_MAX_PROPOSAL_LIFETIME_SEC            (60*60*24*7*4) // Four weeks
#define GRAPHENE_DEFAULT_COMMITTEE_PROPOSAL_REVIEW_PERIOD_SEC (60*60*24) // 1 day
#define GRAPHENE_DEFAULT_MAX_BULK_DISCOUNT_PERCENT            (50*GRAPHENE_1_PERCENT)
#define GRAPHENE_DEFAULT_BULK_DISCOUNT_THRESHOLD_MIN          ( GRAPHENE_BLOCKCHAIN_PRECISION*int64_t(1000) )
#define GRAPHENE_DEFAULT_BULK_DISCOUNT_THRESHOLD_MAX          ( GRAPHENE_DEFAULT_BULK_DISCOUNT_THRESHOLD_MIN*int64_t(100) )
#define GRAPHENE_DEFAULT_CASHBACK_VESTING_PERIOD_SEC          (60*60*24) ///< 1 day
#define GRAPHENE_DEFAULT_BURN_PERCENT_OF_FEE                  (20*GRAPHENE_1_PERCENT)
#define GRAPHENE_WITNESS_PAY_PERCENT_PRECISION                (1000000000)
#define GRAPHENE_DEFAULT_MAX_ASSERT_OPCODE                    1
#define GRAPHENE_DEFAULT_ACCOUNTS_PER_FEE_SCALE               1000
#define GRAPHENE_DEFAULT_ACCOUNT_FEE_SCALE_BITSHIFTS          4
#define GRAPHENE_DEFAULT_MAX_BUYBACK_MARKETS                  4

#define GRAPHENE_MAX_WORKER_NAME_LENGTH                       63

#define GRAPHENE_MAX_URL_LENGTH                               127

#define GRAPHENE_DESCRIBE_LENGTH                              1024 

// counter initialization values used to derive near and far future seeds for shuffling witnesses
// we use the fractional bits of sqrt(2) in hex
#define GRAPHENE_NEAR_SCHEDULE_CTR_IV                    ( (uint64_t( 0x6a09 ) << 0x30)    \
                                                         | (uint64_t( 0xe667 ) << 0x20)    \
                                                         | (uint64_t( 0xf3bc ) << 0x10)    \
                                                         | (uint64_t( 0xc908 )        ) )

// and the fractional bits of sqrt(3) in hex
#define GRAPHENE_FAR_SCHEDULE_CTR_IV                     ( (uint64_t( 0xbb67 ) << 0x30)    \
                                                         | (uint64_t( 0xae85 ) << 0x20)    \
                                                         | (uint64_t( 0x84ca ) << 0x10)    \
                                                         | (uint64_t( 0xa73b )        ) )

/**
 * every second, the fraction of burned core asset which cycles is
 * GRAPHENE_CORE_ASSET_CYCLE_RATE / (1 << GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS)
 */
#define GRAPHENE_CORE_ASSET_CYCLE_RATE                        17
#define GRAPHENE_CORE_ASSET_CYCLE_RATE_BITS                   32

#define GRAPHENE_DEFAULT_WITNESS_PAY_PER_BLOCK            (GRAPHENE_BLOCKCHAIN_PRECISION * int64_t( 10) )
#define GRAPHENE_DEFAULT_WITNESS_PAY_VESTING_SECONDS      (60*60*24)
#define GRAPHENE_DEFAULT_WORKER_BUDGET_PER_DAY            (GRAPHENE_BLOCKCHAIN_PRECISION * int64_t(500) * 1000 )

#define GRAPHENE_DEFAULT_MINIMUM_FEEDS                       7

#define GRAPHENE_MAX_INTEREST_APR                            uint16_t( 10000 )

#define GRAPHENE_RECENTLY_MISSED_COUNT_INCREMENT             4
#define GRAPHENE_RECENTLY_MISSED_COUNT_DECREMENT             3

#define GRAPHENE_CURRENT_DB_VERSION                          "COCOS2.30"

#define GRAPHENE_IRREVERSIBLE_THRESHOLD                      (70 * GRAPHENE_1_PERCENT)//石墨烯不可逆转区块阀值，默认为:7 ,最大为10

/**
 *  Reserved Account IDs with special meaning
 */
///@{
/// Represents the current committee members, two-week review period
#define GRAPHENE_COMMITTEE_ACCOUNT (graphene::chain::account_id_type(0))
#define GRAPHENE_RELAXED_COMMITTEE_ACCOUNT (graphene::chain::account_id_type(1))
/// Represents the current witnesses
#define GRAPHENE_WITNESS_ACCOUNT (graphene::chain::account_id_type(2))
/// Represents the canonical account with NO authority (nobody can access funds in null account)
#define GRAPHENE_NULL_ACCOUNT (graphene::chain::account_id_type(3))
#define GRAPHENE_ACCOUNT (graphene::chain::account_id_type(0))
/// Represents the canonical account with WILDCARD authority (anybody can access funds in temp account)
#define GRAPHENE_TEMP_ACCOUNT (graphene::chain::account_id_type(4))
/// Represents the canonical account for specifying you will vote directly (as opposed to a proxy)
//#define GRAPHENE_PROXY_TO_SELF_ACCOUNT (graphene::chain::account_id_type(4))
/// Sentinel value used in the scheduler.
#define GRAPHENE_NULL_WITNESS (graphene::chain::witness_id_type(0))
#define GRAPHENE_ASSET_GAS (graphene::chain::asset_id_type(1))
///@}
#define WITNESS_CANDIDATE_FREEZE   0//100000*std::pow(10,GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS)
#define COMMITTEE_CANDIDATE_FREEZE  0//100000*std::pow(10,GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS)
#define CANDIDATE_AWARD_BUDGET  0//100000*std::pow(10,GRAPHENE_BLOCKCHAIN_PRECISION_DIGITS)
#define COMMITTEE_PERCENT_OF_CANDIDATE_AWARD               5500
#define UNSUCCESSFUL_CANDIDATES_PERCENT                    1000

#define CONTRACT_BASE_ENV "local baseENV={ cjson={decode=cjson.decode,encode=cjson.encode},date=os.date,import_contract=import_contract,get_account_contract_data=get_account_contract_data, \
                            assert=assert, next=next, pairs=pairs, pcall=pcall, print=print, select=select, tonumber=tonumber, tostring=tostring, type=type,format_vector_with_table=format_vector_with_table ,\
                            unpack=unpack, _VERSION=_VERSION, xpcall=xpcall, string={ byte=string.byte, char=string.char, find=string.find,\
                            format=string.format, gmatch=string.gmatch, gsub=string.gsub, len=string.len, lower=string.lower,  match=string.match, rep=string.rep,\
                            reverse=string.reverse, sub=string.sub, upper=string.upper }, table={ insert=table.insert, maxn=table.maxn, remove=table.remove, sort=table.sort,\
                            concat=table.concat }, math={ abs=math.abs, acos=math.acos, asin=math.asin, atan=math.atan, atan2=math.atan2, ceil=math.ceil, cos=math.cos,\
                            cosh=math.cosh, deg=math.deg, exp=math.exp, floor=math.floor, fmod=math.fmod, frexp=math.frexp, huge=math.huge,  ldexp=math.ldexp, log=math.log,\
                            log10=math.log10, max=math.max, min=math.min, modf=math.modf, pi=math.pi, pow=math.pow, rad=math.rad,sin=math.sin, sinh=math.sinh, sqrt=math.sqrt,\
                            tan=math.tan, tanh=math.tanh }  } return baseENV"
