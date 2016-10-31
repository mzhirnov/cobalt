#pragma once

#include <thread>
#include <future>

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/asio/io_service.hpp>

template <typename Implementation>
class async_renderer_service : public boost::asio::io_service::service {
public:
	static boost::asio::io_service::id id;

	typedef async_renderer_service<Implementation> this_type;

	async_renderer_service(boost::asio::io_service& io_service)
		: boost::asio::io_service::service(io_service)
		, _renderer_io_service(1)
		, _renderer_work(new boost::asio::io_service::work(_renderer_io_service))
		, _renderer_thread(&this_type::entry, this)
	{
	}

	~async_renderer_service() {
		_renderer_work.reset();
		_renderer_thread.join();
	}

	typedef boost::intrusive_ptr<Implementation> implementation_type;

	void construct(implementation_type& impl) {
		impl.reset(new Implementation());
	}

	void destroy(implementation_type& impl) {
		_renderer_io_service.post(std::bind(&Implementation::shutdown, boost::get_pointer(impl)));
		synchronize();
		impl.reset();
	}

	void synchronize() {
		std::packaged_task<void()> task([] {});
		_renderer_io_service.post(std::ref(task));
		task.get_future().get();
	}

	template <typename Handler>
	void execute(Handler&& h) {
		_renderer_io_service.post(std::forward<Handler>(h));
	}

private:
	void shutdown_service() {
	}

	void entry() {
		for (;;) {
			try {
				_renderer_io_service.run();
				break;
			} catch (...) {
				get_io_service().post(std::bind(&std::rethrow_exception, std::current_exception()));
			}
		}
	}

public:
	void initialize(implementation_type& impl, window_handle_type win, boost::system::error_code& ec) {
		ec = boost::system::error_code();
		_renderer_io_service.post(std::bind(&Implementation::initialize, boost::get_pointer(impl), win));
	}

	void begin_scene(implementation_type& impl, boost::system::error_code& ec) {
		ec = boost::system::error_code();
		synchronize();
		_renderer_io_service.post(std::bind(&Implementation::begin_scene, boost::get_pointer(impl)));
	}

	void end_scene(implementation_type& impl, boost::system::error_code& ec) {
		ec = boost::system::error_code();
		_renderer_io_service.post(std::bind(&Implementation::end_scene, boost::get_pointer(impl)));
	}

private:
	boost::asio::io_service _renderer_io_service;
	std::unique_ptr<boost::asio::io_service::work> _renderer_work;
	std::thread _renderer_thread;
};

template <typename Implementation>
boost::asio::io_service::id async_renderer_service<Implementation>::id;
