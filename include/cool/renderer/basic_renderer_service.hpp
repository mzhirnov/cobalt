#pragma once

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/asio/io_service.hpp>

template <typename Implementation>
class basic_renderer_service : public boost::asio::io_service::service {
public:
	static boost::asio::io_service::id id;

	typedef basic_renderer_service<Implementation> this_type;

	basic_renderer_service(boost::asio::io_service& io_service)
		: boost::asio::io_service::service(io_service)
	{
	}

	~basic_renderer_service() {
	}

	typedef boost::intrusive_ptr<Implementation> implementation_type;

	void construct(implementation_type& impl) {
		impl.reset(new Implementation());
	}

	void destroy(implementation_type& impl) {
		impl->shutdown();
		impl.reset();
	}

	void synchronize() {
	}

	template <typename Handler>
	void execute(Handler&& h) {
		h();
	}

private:
	void shutdown_service() {
	}

public:
	void initialize(implementation_type& impl, window_handle_type win, boost::system::error_code& ec) {
		ec = boost::system::error_code();
		impl->initialize(win);
	}

	void begin_scene(implementation_type& impl, boost::system::error_code& ec) {
		ec = boost::system::error_code();
		impl->begin_scene();
	}

	void end_scene(implementation_type& impl, boost::system::error_code& ec) {
		ec = boost::system::error_code();
		impl->end_scene();
	}
};

template <typename Implementation>
boost::asio::io_service::id basic_renderer_service<Implementation>::id;
