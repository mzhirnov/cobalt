#include <system_error>

#include <boost/exception/all.hpp>

inline void throw_error(const std::error_code& ec) {
	if (ec)
		BOOST_THROW_EXCEPTION(std::system_error(ec));
}

inline void throw_error(const std::error_code& ec, const char* message) {
	if (ec)
		BOOST_THROW_EXCEPTION(std::system_error(ec, message));
}
