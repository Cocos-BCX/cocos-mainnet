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
#include <graphene/chain/protocol/operations.hpp>

namespace graphene
{
namespace chain
{
class database;
struct signed_transaction;

/**
    *  Place holder for state tracked while processing a transaction. This class provides helper methods that are
    *  common to many different operations and also tracks which keys have signed the transaction
    */
class transaction_evaluation_state
{
    public:
      transaction_evaluation_state(database *db = nullptr)
          : _db(db) {}

      database &db() const
      {
            assert(_db);
            return *_db;
      }
      vector<operation_result> operation_results;
      const signed_transaction *_trx = nullptr;
      database *_db = nullptr;
      bool is_agreed_task = false;
      uint32_t skip = 0;
      transaction_apply_mode run_mode = production_block_mode;
      flat_set<public_key_type> sigkeys;
};
} // namespace chain
} // namespace graphene
