#include <system_error>

#include <boost/exception/all.hpp>

inline void throw_error(const std::error_code& ec) {
	BOOST_ASSERT(!ec);
	if (ec) {
		boost::throw_exception(std::system_error(ec));
	}
}

inline void throw_error(const std::error_code& ec, const char* message) {
	BOOST_ASSERT(!ec);
	if (ec) {
		boost::throw_exception(std::system_error(ec, message));
	}
}
