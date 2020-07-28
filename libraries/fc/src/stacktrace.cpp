//
// A stacktrace handler for bitshares
//
#include <ostream>
#include <boost/version.hpp>

// only include stacktrace stuff if boost >= 1.65 and not macOS
#if BOOST_VERSION / 100 >= 1065 && !defined(__APPLE__)
#include <signal.h>
#include <fc/log/logger.hpp>
#include <boost/stacktrace.hpp>

namespace fc
{

static void segfault_signal_handler(int signum)
{
   ::signal(signum, SIG_DFL);
   std::stringstream ss;
   ss << boost::stacktrace::stacktrace();
   elog(ss.str());
   ::raise(SIGABRT);
}

void print_stacktrace_on_segfault()
{
   ::signal(SIGSEGV, &segfault_signal_handler);
}

void print_stacktrace(std::ostream& out)
{
   out << boost::stacktrace::stacktrace();
}

}
#else
// Stacktrace output requires Boost 1.65 or above.
// Therefore calls to these methods do nothing.
namespace fc
{
void print_stacktrace_on_segfault() {}
void print_stacktrace(std::ostream& out) {}
}

#endif
