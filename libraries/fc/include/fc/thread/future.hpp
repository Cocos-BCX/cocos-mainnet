#pragma once
#include <fc/time.hpp>
#include <fc/exception/exception.hpp>
#include <fc/thread/spin_yield_lock.hpp>
#include <fc/optional.hpp>

#include <atomic>
#include <memory>

//#define FC_TASK_NAMES_ARE_MANDATORY 1
#ifdef FC_TASK_NAMES_ARE_MANDATORY
# define FC_TASK_NAME_DEFAULT_ARG
#else
# define FC_TASK_NAME_DEFAULT_ARG = "?"
#endif

//#define FC_CANCELATION_REASONS_ARE_MANDATORY 1
#ifdef FC_CANCELATION_REASONS_ARE_MANDATORY
# define FC_CANCELATION_REASON_DEFAULT_ARG
#else
# define FC_CANCELATION_REASON_DEFAULT_ARG = nullptr
#endif

namespace fc {
  struct void_t{};
  class priority;
  class thread;

  namespace detail {
     class completion_handler {
       public:
          virtual ~completion_handler(){};
          virtual void on_complete( const void* v, const fc::exception_ptr& e ) = 0;
     };
     
     template<typename Functor, typename T>
     class completion_handler_impl : public completion_handler {
       public:
         completion_handler_impl( Functor&& f ):_func(std::move(f)){}
         completion_handler_impl( const Functor& f ):_func(f){}
     
         virtual void on_complete( const void* v, const fc::exception_ptr& e ) {
           _func( *static_cast<const T*>(v), e);
         }
       private:
         Functor _func;
     };
     template<typename Functor>
     class completion_handler_impl<Functor,void> : public completion_handler {
       public:
         completion_handler_impl( Functor&& f ):_func(std::move(f)){}
         completion_handler_impl( const Functor& f ):_func(f){}
         virtual void on_complete( const void* v, const fc::exception_ptr& e ) {
           _func(e);
         }
       private:
         Functor _func;
     };
  }

  class promise_base : public std::enable_shared_from_this<promise_base> {
    public:
      typedef std::shared_ptr<promise_base> ptr;
      virtual ~promise_base();

      const char* get_desc()const;
                   
      virtual void cancel(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG);
      bool canceled()const { return _canceled; }
      bool ready()const;
      bool error()const;

      void set_exception( const fc::exception_ptr& e );

    protected:
      promise_base(const char* desc FC_TASK_NAME_DEFAULT_ARG);

      void _wait( const microseconds& timeout_us );
      void _wait_until( const time_point& timeout_us );
      void _notify();
      void _set_value(const void* v);

      void _on_complete( detail::completion_handler* c );

    private:
      void _enqueue_thread();
      void _dequeue_thread();

      friend class  thread;
      friend struct context;
      friend class  thread_d;

      std::atomic<bool>           _ready;
      std::atomic<thread*>        _blocked_thread;
      std::atomic<int32_t>        _blocked_fiber_count;
      time_point                  _timeout;
      fc::exception_ptr           _exceptp;
      bool                        _canceled;
#ifndef NDEBUG
    protected:
      const char*                 _cancellation_reason;
    private:
#endif
      const char*                   _desc;
      std::atomic<detail::completion_handler*> _compl;
  };

  template<typename T = void> 
  class promise : virtual public promise_base {
    public:
      typedef std::shared_ptr< promise<T> > ptr;
      virtual ~promise(){}

      static ptr create( const char* desc FC_TASK_NAME_DEFAULT_ARG )
      {
         return ptr( new promise<T>( desc ) );
      }
      static ptr create( const T& val )
      {
         return ptr( new promise<T>( val ) );
      }
      static ptr create( T&& val )
      {
         return ptr( new promise<T>( std::move(val) ) );
      }

      const T& wait(const microseconds& timeout = microseconds::maximum() ){
        this->_wait( timeout );
        return *result;
      }
      const T& wait_until(const time_point& tp ) {
        this->_wait_until( tp );
        return *result;
      }

      void set_value( const T& v ) {
        result = v;
        _set_value(&*result);
      }

      void set_value( T&& v ) {
        result = std::move(v);
        _set_value(&*result);
      }

      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        _on_complete( new detail::completion_handler_impl<CompletionHandler,T>(std::forward<CompletionHandler>(c)) );
      }
    protected:
      promise( const char* desc ):promise_base(desc){}
      promise( const T& val ){ set_value(val); }
      promise( T&& val ){ set_value(std::move(val) ); }

      optional<T> result;
  };

  template<>
  class promise<void> : virtual public promise_base {
    public:
      typedef std::shared_ptr< promise<void> > ptr;

      virtual ~promise(){}
    
      static ptr create( const char* desc FC_TASK_NAME_DEFAULT_ARG )
      {
         return ptr( new promise<void>( desc ) );
      }
      static ptr create( bool fulfilled, const char* desc FC_TASK_NAME_DEFAULT_ARG )
      {
         return ptr( new promise<void>( fulfilled, desc ) );
      }

      void wait(const microseconds& timeout = microseconds::maximum() ){
        this->_wait( timeout );
      }
      void wait_until(const time_point& tp ) {
        this->_wait_until( tp );
      }

      void set_value(){ this->_set_value(nullptr); }
      void set_value( const void_t&  ) { this->_set_value(nullptr); }

      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        _on_complete( new detail::completion_handler_impl<CompletionHandler,void>(std::forward<CompletionHandler>(c)) );
      }
    protected:
      promise( const char* desc ):promise_base(desc){}
      promise( bool fulfilled, const char* desc ){
          if( fulfilled ) set_value();
      }
  };
  
  /**
   *  @brief a placeholder for the result of an asynchronous operation.
   *
   *  By calling future<T>::wait() you will block the current fiber until 
   *  the asynchronous operation completes.  
   *
   *  If you would like to use an asynchronous interface instead of the synchronous
   *  'wait' method you could specify a CompletionHandler which is a method that takes
   *  two parameters, a const reference to the value and an exception_ptr.  If the
   *  exception_ptr is set, the value reference is invalid and accessing it is
   *  'undefined'.  
   *
   *  Promises have pointer semantics, futures have reference semantics that
   *  contain a shared pointer to a promise.
   */
  template<typename T> 
  class future {
    public:
      future( const typename promise<T>::ptr& p ):m_prom(p){}
      future( typename promise<T>::ptr&& p ):m_prom(std::move(p)){}
      future(const future<T>& f ) : m_prom(f.m_prom){}
      future(){}

      future& operator=(future<T>&& f ) {
        std::swap(m_prom,f.m_prom);
        return *this;
      }

      operator const T&()const { return wait(); }
      
      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      const T& wait( const microseconds& timeout = microseconds::maximum() )const {
           return m_prom->wait(timeout);
      }

      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      const T& wait_until( const time_point& tp )const {
         if( m_prom )
           return m_prom->wait_until(tp);
      }

      bool valid()const { return !!m_prom;       }

      /// @pre valid()
      bool ready()const { return m_prom->ready(); }

      /// @pre valid()
      bool error()const { return m_prom->error(); }

      void cancel(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG) const { if( m_prom ) m_prom->cancel(reason); }
      bool canceled()const { if( m_prom ) return m_prom->canceled(); else return true;}

      void cancel_and_wait(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG)
      {
         if( valid() )
         {
            cancel(reason);
            try
            {
              wait();
            }
            catch (const canceled_exception&)
            {
            }
         }
      }

      /**
       * @pre valid()
       *
       * The given completion handler will be called from some
       * arbitrary thread and should not 'block'. Generally
       * it should post an event or start a new async operation.
       */
      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        m_prom->on_complete( std::forward<CompletionHandler>(c) );
      }
    private:
      friend class thread;
      typename promise<T>::ptr m_prom;
  };

  template<>
  class future<void> {
    public:
      future( const typename promise<void>::ptr& p ):m_prom(p){}
      future( typename promise<void>::ptr&& p ):m_prom(std::move(p)){}
      future(const future<void>& f ) : m_prom(f.m_prom){}
      future(){}

      future& operator=(future<void>&& f ) {
        std::swap(m_prom,f.m_prom);
        return *this;
      }

      
      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      void wait( const microseconds& timeout = microseconds::maximum() ){
         if( m_prom )
           m_prom->wait(timeout);
      }

      /// @pre valid()
      /// @post ready()
      /// @throws timeout
      void wait_until( const time_point& tp ) {
        m_prom->wait_until(tp);
      }

      bool valid()const    { return !!m_prom;           }
      bool canceled()const { return m_prom ? m_prom->canceled() : true; }

      void cancel_and_wait(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG) 
      {
        cancel(reason);
        try
        {
          wait();
        }
        catch (const canceled_exception&)
        {
        }
      }

      /// @pre valid()
      bool ready()const { return m_prom->ready(); }

      /// @pre valid()
      bool error()const { return m_prom->error(); }

      void cancel(const char* reason FC_CANCELATION_REASON_DEFAULT_ARG) const { if( m_prom ) m_prom->cancel(reason); }

      template<typename CompletionHandler>
      void on_complete( CompletionHandler&& c ) {
        m_prom->on_complete( std::forward<CompletionHandler>(c) );
      }

    private:
      friend class thread;
      typename promise<void>::ptr m_prom;
  };
} 

