
#include <fc/filesystem.hpp>
#include <fc/optional.hpp>
#include <fc/variant_object.hpp>
#include <fc/smart_ref_impl.hpp>

#include <graphene/app/application.hpp>

#include <graphene/chain/block_database.hpp>
#include <graphene/chain/database.hpp>
#include <graphene/chain/witness_object.hpp>

#include <graphene/utilities/key_conversion.hpp>

#include <graphene/debug_witness/debug_api.hpp>
#include <graphene/debug_witness/debug_witness.hpp>

namespace graphene { namespace debug_witness {

namespace detail {

class debug_api_impl
{
   public:
      debug_api_impl( graphene::app::application& _app );

      void debug_push_blocks( const std::string& src_filename, uint32_t count );
      void debug_generate_blocks( const std::string& debug_key, uint32_t count );
      void debug_update_object( const fc::variant_object& update );
      //void debug_save_db( std::string db_path );
      void debug_stream_json_objects( const std::string& filename );
      void debug_stream_json_objects_flush();
      std::shared_ptr< graphene::debug_witness_plugin::debug_witness_plugin > get_plugin();

      graphene::app::application& app;
};

debug_api_impl::debug_api_impl( graphene::app::application& _app ) : app( _app )
{}


void debug_api_impl::debug_push_blocks( const std::string& src_filename, uint32_t count )
{
   if( count == 0 )
      return;

   std::shared_ptr< graphene::chain::database > db = app.chain_database();
   fc::path src_path = fc::path( src_filename );
   if( fc::is_directory( src_path ) )
   {
      ilog( "Loading ${n} from block_database ${fn}", ("n", count)("fn", src_filename) );
      graphene::chain::block_database bdb;
      bdb.open( src_path );
      uint32_t first_block = db->head_block_num()+1;
      for( uint32_t i=0; i<count; i++ )
      {
         fc::optional< graphene::chain::signed_block > block = bdb.fetch_by_number( first_block+i );
         if( !block.valid() )
         {
            wlog( "Block database ${fn} only contained ${i} of ${n} requested blocks", ("i", i)("n", count)("fn", src_filename) );
            return;
         }
         try
         {
            db->push_block( *block );
         }
         catch( const fc::exception& e )
         {
            elog( "Got exception pushing block ${bn} : ${bid} (${i} of ${n})", ("bn", block->block_num())("bid", block->make_id())("i", i)("n", count) );
            elog( "Exception backtrace: ${bt}", ("bt", e.to_detail_string()) );
         }
      }
      ilog( "Completed loading block_database successfully" );
      return;
   }else{
      ilog("src_path error");
   }
}

void debug_api_impl::debug_generate_blocks( const std::string& debug_key, uint32_t count )
{
   if( count == 0 )
      return;

   fc::optional<fc::ecc::private_key> debug_private_key = graphene::utilities::wif_to_key( debug_key );
   FC_ASSERT( debug_private_key.valid() );
   graphene::chain::public_key_type debug_public_key = debug_private_key->get_public_key();

   std::shared_ptr< graphene::chain::database > db = app.chain_database();
   for( uint32_t i=0; i<count; i++ )
   {
      graphene::chain::witness_id_type scheduled_witness = db->get_scheduled_witness( 1 );
      fc::time_point_sec scheduled_time = db->get_slot_time( 1 );
      graphene::chain::public_key_type scheduled_key = scheduled_witness( *db ).signing_key;
      if( scheduled_key != debug_public_key )
      {
         ilog( "Modified key for witness ${w}", ("w", scheduled_witness) );
         fc::mutable_variant_object update;
         update("_action", "update")("id", scheduled_witness)("signing_key", debug_public_key);
         db->debug_update( update );
      }
      db->generate_block( scheduled_time, scheduled_witness, *debug_private_key, graphene::chain::database::skip_nothing );
   }
}

void debug_api_impl::debug_update_object( const fc::variant_object& update )
{
   std::shared_ptr< graphene::chain::database > db = app.chain_database();
   db->debug_update( update );
}

std::shared_ptr< graphene::debug_witness_plugin::debug_witness_plugin > debug_api_impl::get_plugin()
{
   return app.get_plugin< graphene::debug_witness_plugin::debug_witness_plugin >( "debug_witness" );
}

void debug_api_impl::debug_stream_json_objects( const std::string& filename )
{
   get_plugin()->set_json_object_stream( filename );
}

void debug_api_impl::debug_stream_json_objects_flush()
{
   get_plugin()->flush_json_object_stream();
}

} // detail

debug_api::debug_api( graphene::app::application& app )
{
   my = std::make_shared< detail::debug_api_impl >(app);
}

void debug_api::debug_push_blocks( std::string source_filename, uint32_t count )
{
   my->debug_push_blocks( source_filename, count );
}

void debug_api::debug_generate_blocks( std::string debug_key, uint32_t count )
{
   my->debug_generate_blocks( debug_key, count );
}

void debug_api::debug_update_object( fc::variant_object update )
{
   my->debug_update_object( update );
}

void debug_api::debug_stream_json_objects( std::string filename )
{
   my->debug_stream_json_objects( filename );
}

void debug_api::debug_stream_json_objects_flush()
{
   my->debug_stream_json_objects_flush();
}


} } // graphene::debug_witness
