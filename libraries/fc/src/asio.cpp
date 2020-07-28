#include <fc/asio.hpp>
#include <fc/thread/thread.hpp>
#include <boost/thread.hpp>
#include <fc/log/logger.hpp>
#include <fc/exception/exception.hpp>
#include <boost/scope_exit.hpp>
#include <algorithm>
#include <thread>

namespace fc {
  namespace asio {
    namespace detail {

      read_write_handler::read_write_handler(const promise<size_t>::ptr& completion_promise) :
        _completion_promise(completion_promise)
      {
        // assert(false); // to detect anywhere we're not passing in a shared buffer
      }
      void read_write_handler::operator()(const boost::system::error_code& ec, size_t bytes_transferred)
      {
        // assert(false); // to detect anywhere we're not passing in a shared buffer
        if( !ec )
          _completion_promise->set_value(bytes_transferred);
        else if( ec == boost::asio::error::eof  )
          _completion_promise->set_exception( fc::exception_ptr( new fc::eof_exception( FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
        else
          _completion_promise->set_exception( fc::exception_ptr( new fc::exception( FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
      }
      read_write_handler_with_buffer::read_write_handler_with_buffer(const promise<size_t>::ptr& completion_promise,
                                                                     const std::shared_ptr<const char>& buffer) :
        _completion_promise(completion_promise),
        _buffer(buffer)
      {}
      void read_write_handler_with_buffer::operator()(const boost::system::error_code& ec, size_t bytes_transferred)
      {
        if( !ec )
          _completion_promise->set_value(bytes_transferred);
        else if( ec == boost::asio::error::eof  )
          _completion_promise->set_exception( fc::exception_ptr( new fc::eof_exception( FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
        else
          _completion_promise->set_exception( fc::exception_ptr( new fc::exception( FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
      }

        void read_write_handler_ec( promise<size_t>* p, boost::system::error_code* oec, const boost::system::error_code& ec, size_t bytes_transferred ) {
            p->set_value(bytes_transferred);
            *oec = ec;
        }
        void error_handler( const promise<void>::ptr& p,
                              const boost::system::error_code& ec ) {
            if( !ec )
              p->set_value();
            else
            {
                if( ec == boost::asio::error::eof  )
                {
                  p->set_exception( fc::exception_ptr( new fc::eof_exception(
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
                else
                {
                  p->set_exception( fc::exception_ptr( new fc::exception(
                          FC_LOG_MESSAGE( error, "${message} ", ("message", boost::system::system_error(ec).what())) ) ) );
                }
            }
        }

        void error_handler_ec( promise<boost::system::error_code>* p,
                              const boost::system::error_code& ec ) {
            p->set_value(ec);
        }

        template<typename EndpointType, typename IteratorType>
        void resolve_handler(
                             const typename promise<std::vector<EndpointType> >::ptr& p,
                             const boost::system::error_code& ec,
                             IteratorType itr) {
            if( !ec ) {
                std::vector<EndpointType> eps;
                while( itr != IteratorType() ) {
                    eps.push_back(*itr);
                    ++itr;
                }
                p->set_value( eps );
            } else {
                p->set_exception(
                    fc::exception_ptr( new fc::exception(
                        FC_LOG_MESSAGE( error, "process exited with: ${message} ",
                                        ("message", boost::system::system_error(ec).what())) ) ) );
            }
        }
    }

    uint16_t fc::asio::default_io_service_scope::num_io_threads = 0;

    /***
     * @brief set the default number of threads for the io service
     *
     * Sets the number of threads for the io service. This will throw
     * an exception if called more than once.
     *
     * @param num_threads the number of threads
     */
    void default_io_service_scope::set_num_threads(uint16_t num_threads) {
       FC_ASSERT(num_io_threads == 0);
       num_io_threads = num_threads;
    }

    uint16_t default_io_service_scope::get_num_threads() { return num_io_threads; }

    /***
     * Default constructor
     */
    default_io_service_scope::default_io_service_scope()
    {
       io           = new boost::asio::io_service();
       the_work     = new boost::asio::io_service::work(*io);

       if( num_io_threads == 0 )
       {
          // the default was not set by the configuration. Determine a good
          // number of threads. Minimum of 8, maximum of hardware_concurrency
          num_io_threads = std::max( boost::thread::hardware_concurrency(), 8U );
       }

       for( uint16_t i = 0; i < num_io_threads; ++i )
       {
          asio_threads.push_back( new boost::thread( [i,this]()
                {
                 fc::thread::current().set_name( "fc::asio worker #" + fc::to_string(i) );
                 
                 BOOST_SCOPE_EXIT(void)
                 {
                    fc::thread::cleanup();
                 } 
                 BOOST_SCOPE_EXIT_END

                 while (!io->stopped())
                 {
                    try
                    {
                       io->run();
                    }
                    catch (const fc::exception& e)
                    {
                       elog("Caught unhandled exception in asio service loop: ${e}", ("e", e));
                    }
                    catch (const std::exception& e)
                    {
                       elog("Caught unhandled exception in asio service loop: ${e}", ("e", e.what()));
                    }
                    catch (...)
                    {
                       elog("Caught unhandled exception in asio service loop");
                    }
                 }
                }) );
       } // build thread loop
    } // end of constructor

    /***
     * destructor
     */
    default_io_service_scope::~default_io_service_scope()
    {
       delete the_work;
       io->stop();
       for( auto asio_thread : asio_threads )
       {
          asio_thread->join();
       }
       delete io;
       for( auto asio_thread : asio_threads )
       {
          delete asio_thread;
       }
    } // end of destructor

    /***
     * @brief create an io_service
     * @returns the io_service
     */
    boost::asio::io_service& default_io_service() {
        static default_io_service_scope fc_asio_service[1];
        return *fc_asio_service[0].io;
    }

    namespace tcp {
      std::vector<boost::asio::ip::tcp::endpoint> resolve( const std::string& hostname, const std::string& port)
      {
        try
        {
          resolver res( fc::asio::default_io_service() );
          promise<std::vector<boost::asio::ip::tcp::endpoint> >::ptr p = promise<std::vector<boost::asio::ip::tcp::endpoint> >::create("tcp::resolve completion");
          res.async_resolve( boost::asio::ip::tcp::resolver::query(hostname,port),
                            boost::bind( detail::resolve_handler<boost::asio::ip::tcp::endpoint,resolver_iterator>, p, _1, _2 ) );
          return p->wait();
        }
        FC_RETHROW_EXCEPTIONS(warn, "")
      }
    }
    namespace udp {
      std::vector<udp::endpoint> resolve( resolver& r, const std::string& hostname, const std::string& port)
      {
        try
        {
          resolver res( fc::asio::default_io_service() );
          promise<std::vector<endpoint> >::ptr p = promise<std::vector<endpoint> >::create("udp::resolve completion");
          res.async_resolve( resolver::query(hostname,port),
                              boost::bind( detail::resolve_handler<endpoint,resolver_iterator>, p, _1, _2 ) );
          return p->wait();
        }
        FC_RETHROW_EXCEPTIONS(warn, "")
      }
    }

} } // namespace fc::asio
