#include <fc/thread/future.hpp>
#include <fc/thread/spin_yield_lock.hpp>
#include <fc/thread/thread.hpp>
#include <fc/thread/unique_lock.hpp>
#include <fc/exception/exception.hpp>

#include <boost/assert.hpp>

namespace fc {

  promise_base::promise_base( const char* desc )
  :_ready(false),
   _blocked_thread(nullptr),
   _blocked_fiber_count(0),
   _timeout(time_point::maximum()),
   _canceled(false),
#ifndef NDEBUG
   _cancellation_reason(nullptr),
#endif
   _desc(desc),
   _compl(nullptr)
  { }

  promise_base::~promise_base() { }

  const char* promise_base::get_desc()const{
    return _desc; 
  }
               
  void promise_base::cancel(const char* reason /* = nullptr */){
//      wlog("${desc} canceled!", ("desc", _desc? _desc : ""));
      _canceled = true;
#ifndef NDEBUG
      _cancellation_reason = reason;
#endif
    }
  bool promise_base::ready()const {
    return _ready.load();
  }
  bool promise_base::error()const {
    return std::atomic_load( &_exceptp ) != nullptr;
  }

  void promise_base::set_exception( const fc::exception_ptr& e ){
    std::atomic_store( &_exceptp, e );
    _set_value(nullptr);
  }

  void promise_base::_wait( const microseconds& timeout_us ){
     if( timeout_us == microseconds::maximum() ) 
       _wait_until( time_point::maximum() );
     else 
       _wait_until( time_point::now() + timeout_us );
  }
  void promise_base::_wait_until( const time_point& timeout_us ){
    if( _ready.load() ) {
       fc::exception_ptr ex = std::atomic_load( &_exceptp );
       if( ex )
          ex->dynamic_rethrow_exception();
       return;
    }
    _enqueue_thread();
    // Need to check _ready again to avoid a race condition.
    if( _ready.load() )
    {
       _dequeue_thread();
       return _wait_until( timeout_us ); // this will simply return or throw _exceptp
    }

    std::exception_ptr e;
    //
    // Create shared_ptr to take ownership of this; i.e. this will
    // be deleted when p_this goes out of scope.  Consequently,
    // it would be Very Bad to let p_this go out of scope
    // before we're done reading/writing instance variables!
    // See https://github.com/cryptonomex/graphene/issues/597
    //
    ptr p_this = shared_from_this();
    try
    {
       //
       // We clone p_this here because the wait_until() API requires us
       // to use std::move().  I.e. wait_until() takes ownership of any
       // pointer passed to it.  Since we want to keep ownership ourselves,
       // we need to have two shared_ptr's to this:
       //
       // - p_this to keep this alive until the end of the current function
       // - p_this2 to be owned by wait_until() as the wait_until() API requires
       //
       ptr p_this2 = p_this;
       thread::current().wait_until( std::move( p_this2 ), timeout_us );
    }
    catch (...) { e = std::current_exception(); }

    _dequeue_thread();

    if( e ) std::rethrow_exception(e);

    if( _ready.load() ) return _wait_until( timeout_us ); // this will simply return or throw _exceptp

    FC_THROW_EXCEPTION( timeout_exception, "" );
  }
  void promise_base::_enqueue_thread(){
     _blocked_fiber_count.fetch_add( 1 );
     thread* blocked_thread = _blocked_thread.load();
     // only one thread can wait on a promise at any given time
     do
        assert( !blocked_thread || blocked_thread == &thread::current() );
     while( !_blocked_thread.compare_exchange_weak( blocked_thread, &thread::current() ) );
  }
  void promise_base::_dequeue_thread(){ 
    if( _blocked_fiber_count.fetch_add( -1 ) == 1 )
       _blocked_thread.store( nullptr );
  }
  void promise_base::_notify(){
    // copy _blocked_thread into a local so that if the thread unblocks (e.g., 
    // because of a timeout) before we get a chance to notify it, we won't be
    // calling notify on a null pointer
    thread* blocked_thread = _blocked_thread.load();
    if( blocked_thread ) 
      blocked_thread->notify( shared_from_this() );
  }

  void promise_base::_set_value(const void* s){
     bool ready = false;
     if( !_ready.compare_exchange_strong( ready, true ) ) //don't allow promise to be set more than once
        return;
     _notify();
     auto* hdl = _compl.load();
     if( nullptr != hdl )
        hdl->on_complete( s, std::atomic_load( &_exceptp ) );
  }

  void promise_base::_on_complete( detail::completion_handler* c ) {
     auto* hdl = _compl.load();
     while( !_compl.compare_exchange_weak( hdl, c ) );
     delete hdl;
  }
}

