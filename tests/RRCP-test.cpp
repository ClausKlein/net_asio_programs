#include <boost/ut.hpp>  // import boost.ut;
#include <iomanip>  // use std::quoted
#include <sstream>

#include "rrcp_helper.hpp"

namespace ut = boost::ut;

ut::suite errors = []
{
  using namespace ut;
  using namespace std::string_literals;

  "throws"_test = [] { expect(throws([] { throw 0; })); };

  "doesn't throw"_test = [] { expect(nothrow([] {})); };

  "basic_quoteing"_test = []
  {
    std::string binary{"\nAB_(\0\001\002\003\004\005\006\a\b\n\r\t\v\x1b\20\'\"\?)-CD\r"s};
    auto quoted = RRCP::char2esc(binary);

    // NOTE: std::quoted works only with std::stringstream
#if defined(BOOST_UT_HAS_FORMAT) && defined(FIXME)
    std::ostringstream binary_bin;
    binary_bin << std::quoted(binary);
    ut::log("{} {}\n", binary.length(), binary_bin.str());

    std::ostringstream quoted_bin;
    quoted_bin << std::quoted(quoted);
    ut::log("{} {}\n", quoted.length(), quoted_bin.str());
#endif

    expect(binary == RRCP::esc2char(quoted));
    expect(binary.length() < quoted.length());
    expect(binary.length() == 28);
    expect(quoted.length() == 33);
  };
};

int main() {}
