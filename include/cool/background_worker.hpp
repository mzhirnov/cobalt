#pragma once

// Classes in this file:
//	background_worker

#include "cool/common.hpp"

#include <atomic>

#include <boost/unordered_map.hpp>
#include <boost/thread.hpp>
#include <boost/any.hpp>
#include <boost/asio/io_service.hpp>

class application_component;

/// result of asynchronous operation
class async_result : public thread_safe_ref_counter<async_result> {
public:
	virtual ~async_result() {}

	virtual bool busy() const = 0;
	virtual void wait() = 0;
	virtual void cancel() = 0;

	virtual bool cancelled() const = 0;
	virtual boost::any get() const = 0;
};

typedef boost::intrusive_ptr<async_result> async_result_ptr;

/// background worker on thread pool
class background_worker {
public:
	/// asynchronous operation result
	class async_result {
	public:
		async_result(bool cancelled, const boost::shared_future<boost::any>& future)
			: cancelled(cancelled), future(future)
		{}	

		bool was_cancelled() const { return cancelled; }
		boost::any get_result() const { return future.get(); }

	private:
		bool cancelled;
		mutable boost::shared_future<boost::any> future;
	};

	typedef size_t work_id;

	/// asynchronous operation callback
	struct callback {
		virtual ~callback() {}
		virtual void on_pre_execute(work_id id, background_worker* worker) = 0;
		virtual boost::any on_execute_async(work_id id, background_worker* worker) = 0;
		virtual void on_progress_changed(work_id id, background_worker* worker, int progress) = 0;
		virtual void on_post_execute(work_id id, background_worker* worker, const async_result& result) = 0;
	};

public:
	typedef std::function<boost::any(work_id, background_worker*)> async_handler_type;
	typedef std::function<void(work_id, const async_result&)> result_handler_type;

	explicit background_worker(size_t number_of_threads = 1);

	~background_worker();

	void initialize();
	void update();

	work_id run_async(async_handler_type handler, result_handler_type result);
	work_id run_async(callback* callback); ///< elaborate version of run_async

	void interruption_point() const;

	void report_progress(work_id id, int progress);

	bool is_busy_any() const;
	bool is_busy(work_id id) const;

	void cancel_all();
	void cancel(work_id id);
	bool is_cancelled(work_id id) const;

	void wait_all() const;
	void wait(work_id id) const;

private:
	work_id get_next_work_id() const;

	boost::any launch_work(work_id id, background_worker* worker, async_handler_type handler);

	class work;
	work* find_work(work_id id) const;

private:
	size_t _number_of_threads;

	class work {
	public:
		work_id id;
		boost::thread::id thread_id;
		int progress;
		bool progress_changed;
		bool cancelled;
		boost::shared_future<boost::any> future;
		result_handler_type result;
		background_worker::callback* callback;
		boost::shared_mutex mutex;

		work(work_id id, boost::shared_future<boost::any>& future, result_handler_type result)
			: id(id), progress(0), progress_changed(false), cancelled(false), future(future), result(result), callback(nullptr)
		{}

		work(work_id id, boost::shared_future<boost::any>& future, background_worker::callback* callback)
			: id(id), progress(0), progress_changed(false), cancelled(false), future(future), callback(callback)
		{}

		DISALLOW_COPY_AND_ASSIGN(work);
	};

	mutable std::atomic<work_id> _last_work_id;
	
	typedef std::list<work> Works;
	Works _works;
	mutable boost::shared_mutex _mutex;

	boost::asio::io_service _io;
	boost::asio::io_service::work _io_work;

	typedef boost::unordered_map<boost::thread::id, boost::thread*> thread_map_type;
	thread_map_type _thread_map;
	boost::thread_group _thread_pool;

	application_component* _component; ///< application component which calls initialize() and update()

	DISALLOW_COPY_AND_ASSIGN(background_worker);
};

typedef boost::intrusive_ptr<background_worker> background_worker_ptr;
