/*
 * Copyright (c) 2017 Cryptonomex, Inc., and contributors.
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

#include <graphene/elasticsearch/elasticsearch_plugin.hpp>

//#include <graphene/app/impacted.hpp>
#include <graphene/chain/db_notify.hpp>

#include <graphene/chain/account_evaluator.hpp>
#include <graphene/chain/account_object.hpp>
#include <graphene/chain/config.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/evaluator.hpp>
#include <graphene/chain/operation_history_object.hpp>
#include <graphene/chain/transaction_evaluation_state.hpp>

#include <fc/smart_ref_impl.hpp>
#include <fc/thread/thread.hpp>

#include <curl/curl.h>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string.hpp>
#include <regex>

namespace graphene { namespace elasticsearch {

namespace detail
{

class elasticsearch_plugin_impl
{
   public:
      elasticsearch_plugin_impl(elasticsearch_plugin& _plugin)
         : _self( _plugin )
      {  curl = curl_easy_init(); }
      virtual ~elasticsearch_plugin_impl();

      void update_account_histories( const signed_block& b );

      graphene::chain::database& database()
      {
         return _self.database();
      }

      elasticsearch_plugin& _self;
      primary_index< operation_history_index >* _oho_index;

      std::string _elasticsearch_node_url = "http://localhost:9200/";
      uint32_t _elasticsearch_bulk_replay = 10000;
      uint32_t _elasticsearch_bulk_sync = 100;
      bool _elasticsearch_logs = true;
      bool _elasticsearch_visitor = false;
      CURL *curl; // curl handler
      vector <string> bulk; // vector of op lines
   private:
      void add_elasticsearch( const account_id_type account_id, const optional<operation_history_object>& oho, const signed_block& b );
      void createBulkLine(account_transaction_history_object ath, operation_history_struct os, int op_type, block_struct bs, visitor_struct vs);
      void sendBulk(std::string _elasticsearch_node_url, bool _elasticsearch_logs);

};

elasticsearch_plugin_impl::~elasticsearch_plugin_impl()
{
   return;
}

void elasticsearch_plugin_impl::update_account_histories( const signed_block& b )
{
   graphene::chain::database& db = database();
   const vector<optional< operation_history_object > >& hist = db.get_applied_operations();
   for( const optional< operation_history_object >& o_op : hist ) {
      optional <operation_history_object> oho;

      auto create_oho = [&]() {
         return optional<operation_history_object>(
               db.create<operation_history_object>([&](operation_history_object &h) {
                  if (o_op.valid())
                  {
                     h.op           = o_op->op;
                     h.result       = o_op->result;
                     h.block_num    = o_op->block_num;
                     h.trx_in_block = o_op->trx_in_block;
                     h.op_in_trx    = o_op->op_in_trx;
                     h.virtual_op   = o_op->virtual_op;
                  }
               }));
      };

      if( !o_op.valid() ) {
         _oho_index->use_next_id();
         continue;
      }
      oho = create_oho();

      const operation_history_object& op = *o_op;

      // get the set of accounts this operation applies to
      flat_set<account_id_type> impacted;
      vector<authority> other;
      operation_get_required_authorities( op.op, impacted, impacted, other ); // fee_payer is added here

      if( op.op.which() == operation::tag< account_create_operation >::value )
         impacted.insert( op.result.get<object_id_result>().result );
      else
         {  
            //graphene::app::operation_get_impacted_accounts( op.op, impacted );
            graphene::chain::operation_get_impacted_accounts( op.op, impacted );
            //graphene::chain::get_impacted_accounts_from_operation_reslut(op.result,impacted);
         }

      for( auto& a : other )
         for( auto& item : a.account_auths )
            impacted.insert( item.first );

      for( auto& account_id : impacted )
      {
         add_elasticsearch( account_id, oho, b );
      }
   }
}

void elasticsearch_plugin_impl::add_elasticsearch( const account_id_type account_id, const optional <operation_history_object>& oho, const signed_block& b)
{
   graphene::chain::database& db = database();
   const auto &stats_obj = account_id(db).statistics(db);

   // add new entry
   const auto &ath = db.create<account_transaction_history_object>([&](account_transaction_history_object &obj) {
      obj.operation_id = oho->id;
      obj.account = account_id;
      obj.sequence = stats_obj.total_ops + 1;
      obj.next = stats_obj.most_recent_op;
   });

   // keep stats growing as no op will be removed
   db.modify(stats_obj, [&](account_statistics_object &obj) {
      obj.most_recent_op = ath.id;
      obj.total_ops = ath.sequence;
   });

   // operation_type
   int op_type = -1;
   if (!oho->id.is_null())
      op_type = oho->op.which();

   // operation history data
   operation_history_struct os;
   os.trx_in_block = oho->trx_in_block;
   os.op_in_trx = oho->op_in_trx;
   os.operation_result = fc::json::to_string(oho->result);
   os.virtual_op = oho->virtual_op;
   os.op = fc::json::to_string(oho->op);

   // visitor data
   visitor_struct vs;
   if(_elasticsearch_visitor) {
      operation_visitor o_v;
      oho->op.visit(o_v);
      vs.transfer_data.asset = o_v.transfer_asset_id;
      vs.transfer_data.amount = o_v.transfer_amount;
      vs.transfer_data.from = o_v.transfer_from;
      vs.transfer_data.to = o_v.transfer_to;
   }

   // block data
   std::string trx_id = "";
   if(!b.transactions.empty() && oho->trx_in_block < b.transactions.size()) {
      trx_id = b.transactions[oho->trx_in_block].first.str();
   }
   block_struct bs;
   bs.block_num = b.block_num();
   bs.block_time = b.timestamp;
   bs.trx_id = trx_id;

   // check if we are in replay or in sync and change number of bulk documents accordingly
   uint32_t limit_documents = 0;
   if((fc::time_point::now() - b.timestamp) < fc::seconds(30))
      limit_documents = _elasticsearch_bulk_sync;
   else
      limit_documents = _elasticsearch_bulk_replay;

   createBulkLine(ath, os, op_type, bs, vs); // we have everything, creating bulk line

   if (curl && bulk.size() >= limit_documents) { // we are in bulk time, ready to add data to elasticsearech
      sendBulk(_elasticsearch_node_url, _elasticsearch_logs);
   }

   // remove everything except current object from ath
   const auto &his_idx = db.get_index_type<account_transaction_history_index>();
   const auto &by_seq_idx = his_idx.indices().get<by_seq>();
   auto itr = by_seq_idx.lower_bound(boost::make_tuple(account_id, 0));
   if (itr != by_seq_idx.end() && itr->account == account_id && itr->id != ath.id) 
   {
      // if found, remove the entry
      const auto remove_op_id = itr->operation_id;
      const auto itr_remove = itr;
      ++itr;
      db.remove( *itr_remove );
      // modify previous node's next pointer
      // this should be always true, but just have a check here
      if( itr != by_seq_idx.end() && itr->account == account_id )
      {
         db.modify( *itr, [&]( account_transaction_history_object& obj ){
            obj.next = account_transaction_history_id_type();
         });
      }
      // do the same on oho
      const auto &by_opid_idx = his_idx.indices().get<by_opid>();
      if (by_opid_idx.find(remove_op_id) == by_opid_idx.end()) {
         db.remove(remove_op_id(db));
      }
   }
}

void elasticsearch_plugin_impl::createBulkLine(account_transaction_history_object ath, operation_history_struct os, int op_type, block_struct bs, visitor_struct vs)
{
   bulk_struct bulks;
   bulks.account_history = ath;
   bulks.operation_history = os;
   bulks.operation_type = op_type;
   bulks.block_data = bs;
   bulks.additional_data = vs;

   std::string alltogether = fc::json::to_string(bulks);

   auto block_date = bulks.block_data.block_time.to_iso_string();
   std::vector<std::string> parts;
   boost::split(parts, block_date, boost::is_any_of("-"));
   std::string index_name = "graphene-" + parts[0] + "-" + parts[1];

   // bulk header before each line, op_type = create to avoid dups, index id will be ath id(2.9.X).
   std::string _id = fc::json::to_string(ath.id);
   bulk.push_back("{ \"index\" : { \"_index\" : \""+index_name+"\", \"_type\" : \"data\", \"op_type\" : \"create\", \"_id\" : "+_id+" } }"); // header
   bulk.push_back(alltogether);
}

void elasticsearch_plugin_impl::sendBulk(std::string _elasticsearch_node_url, bool _elasticsearch_logs)
{

   // curl buffers to read
   std::string readBuffer;
   std::string readBuffer_logs;

   std::string bulking = "";

   bulking = boost::algorithm::join(bulk, "\n");
   bulking = bulking + "\n";
   bulk.clear();

   //wlog((bulking));

   struct curl_slist *headers = NULL;
   curl_slist_append(headers, "Content-Type: application/json");
   std::string url = _elasticsearch_node_url + "_bulk";
   curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
   curl_easy_setopt(curl, CURLOPT_POST, true);
   curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
   curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bulking.c_str());
   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
   curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&readBuffer);
   curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcrp/0.1");
   //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
   curl_easy_perform(curl);

   long http_code = 0;
   curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
   if(http_code == 200) {
      // all good, do nothing
   }
   else if(http_code == 429) {
      // repeat request?
   }
   else {
      // exit everything ?
   }

   if(_elasticsearch_logs) {
      auto logs = readBuffer;
      // do logs
      std::string url_logs = _elasticsearch_node_url + "logs/data/";
      curl_easy_setopt(curl, CURLOPT_URL, url_logs.c_str());
      curl_easy_setopt(curl, CURLOPT_POST, true);
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, logs.c_str());
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *) &readBuffer_logs);
      curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcrp/0.1");
      //curl_easy_setopt(curl, CURLOPT_VERBOSE, true);
      //ilog("log here curl: ${output}", ("output", readBuffer_logs));
      curl_easy_perform(curl);

      http_code = 0;
      curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
      if(http_code == 200) {
         // all good, do nothing
      }
      else if(http_code == 429) {
         // repeat request?
      }
      else {
         // exit everything ?
      }
   }
}

} // end namespace detail

elasticsearch_plugin::elasticsearch_plugin() :
   my( new detail::elasticsearch_plugin_impl(*this) )
{
}

elasticsearch_plugin::~elasticsearch_plugin()
{
}

std::string elasticsearch_plugin::plugin_name()const
{
   return "elasticsearch";
}
std::string elasticsearch_plugin::plugin_description()const
{
   return "Stores account history data in elasticsearch database(EXPERIMENTAL).";
}

void elasticsearch_plugin::plugin_set_program_options(
   boost::program_options::options_description& cli,
   boost::program_options::options_description& cfg
   )
{
   cli.add_options()
         ("elasticsearch-node-url", boost::program_options::value<std::string>(), "Elastic Search database node url")
         ("elasticsearch-bulk-replay", boost::program_options::value<uint32_t>(), "Number of bulk documents to index on replay(5000)")
         ("elasticsearch-bulk-sync", boost::program_options::value<uint32_t>(), "Number of bulk documents to index on a syncronied chain(10)")
         ("elasticsearch-logs", boost::program_options::value<bool>(), "Log bulk events to database")
         ("elasticsearch-visitor", boost::program_options::value<bool>(), "Use visitor to index additional data(slows down the replay)")
         ;
   cfg.add(cli);
}

void elasticsearch_plugin::plugin_initialize(const boost::program_options::variables_map& options)
{
   database().applied_block.connect( [&]( const signed_block& b){ my->update_account_histories(b); } );
   my->_oho_index = database().add_index< primary_index< operation_history_index > >();
   database().add_index< primary_index< account_transaction_history_index > >();

   if (options.count("elasticsearch-node-url")) {
      my->_elasticsearch_node_url = options["elasticsearch-node-url"].as<std::string>();
   }
   if (options.count("elasticsearch-bulk-replay")) {
      my->_elasticsearch_bulk_replay = options["elasticsearch-bulk-replay"].as<uint32_t>();
   }
   if (options.count("elasticsearch-bulk-sync")) {
      my->_elasticsearch_bulk_sync = options["elasticsearch-bulk-sync"].as<uint32_t>();
   }
   if (options.count("elasticsearch-logs")) {
      my->_elasticsearch_logs = options["elasticsearch-logs"].as<bool>();
   }
   if (options.count("elasticsearch-visitor")) {
      my->_elasticsearch_visitor = options["elasticsearch-visitor"].as<bool>();
   }
}

void elasticsearch_plugin::plugin_startup()
{
}

} }
