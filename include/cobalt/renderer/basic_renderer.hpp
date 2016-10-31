#pragma once

#include <boost/asio/basic_io_object.hpp>

template <typename Service>
class basic_renderer : public boost::asio::basic_io_object<Service> {
public:
	explicit basic_renderer(boost::asio::io_service& io_service)
		: boost::asio::basic_io_object<Service>(io_service)
	{
	}

	void synchronize() {
		this->get_service().synchronize();
	}

	template <typename Handler>
	void execute(Handler&& h) {
		this->get_service().execute(std::forward<Handler>(h));
	}

	void initialize(window_handle_type win) {
		boost::system::error_code ec;
		this->get_service().initialize(this->get_implementation(), win, ec);
		boost::asio::detail::throw_error(ec);
	}

	void begin_scene() {
		boost::system::error_code ec;
		this->get_service().begin_scene(this->get_implementation(), ec);
		boost::asio::detail::throw_error(ec);
	}

	void end_scene() {
		boost::system::error_code ec;
		this->get_service().end_scene(this->get_implementation(), ec);
		boost::asio::detail::throw_error(ec);
	}
};
