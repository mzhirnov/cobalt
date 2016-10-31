#include "pch.h"
#include "cool/background_worker.hpp"
#include "cool/application.hpp"

#include <boost/smart_ptr/make_shared.hpp>

namespace {

class worker_component : public application_component {
public:
	explicit worker_component(background_worker* worker)
		: application_component(update_order::earliest)
		, _worker(worker)
	{}

	void initialize() { _worker->initialize(); }
	void update() { _worker->update(); }

private:
	background_worker* _worker;
};

} // anonymous nsmaspace

background_worker::background_worker(size_t number_of_threads)
	: _io(number_of_threads)
	, _io_work(_io)
	, _last_work_id(0)
	, _number_of_threads(number_of_threads)
{
	_component = application::get_instance()->add_component(new worker_component(this));
}

background_worker::~background_worker() {
	application::get_instance()->remove_component(_component);

	_io.stop();
	_thread_pool.join_all();

	update();
}

background_worker::work_id background_worker::run_async(async_handler_type handler, result_handler_type result) {
	work_id id = get_next_work_id();

	typedef boost::packaged_task<boost::any> Task;
	
	auto task = std::make_shared<Task>(std::bind(&background_worker::launch_work, this, id, this, handler));
	
	boost::shared_future<boost::any> future = task->get_future();
	{
		boost::unique_lock<boost::shared_mutex> lock(_mutex);
		_works.emplace_back(id, future, result);
	}

	_io.post(boost::bind(&Task::operator(), task));

	return id;
}

background_worker::work_id background_worker::run_async(callback* callback) {
	BOOST_ASSERT(!!callback);

	work_id id = get_next_work_id();

	// If it throws here, it's ok
	callback->on_pre_execute(id, this);

	async_handler_type handler = std::bind(&callback::on_execute_async, callback, std::placeholders::_1, std::placeholders::_2);

	typedef boost::packaged_task<boost::any> Task;

	auto task = std::make_shared<Task>(std::bind(&background_worker::launch_work, this, id, this, handler));
	
	boost::shared_future<boost::any> future = task->get_future();
	{
		boost::unique_lock<boost::shared_mutex> lock(_mutex);
		_works.emplace_back(id, future, callback);
	}

	_io.post(boost::bind(&Task::operator(), task));

	return id;
}

background_worker::work_id background_worker::get_next_work_id() const {
	return ++_last_work_id;
}

boost::any background_worker::launch_work(work_id id, background_worker* worker, async_handler_type handler) {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);

	work* work = find_work(id);
	if (!work) {
		return boost::any();
	}

	{
		// If work is already cancelled, don't launch it
		boost::upgrade_lock<boost::shared_mutex> work_lock(work->mutex);
		if (work->cancelled) {
			return boost::any();
		}

		// Set work thread id, which will mean we're running in this thread
		boost::upgrade_to_unique_lock<boost::shared_mutex> upgrade(work_lock);
		work->thread_id = boost::this_thread::get_id();
	}

	boost::any result;

	try {
		// Execute work handler
		result = handler(id, worker);
	} catch (const boost::thread_interrupted&) {
		// Interruption means cancellation
		boost::unique_lock<boost::shared_mutex> work_lock(work->mutex);
		work->cancelled = true;
	}

	{
		// Reset thread id, which means we are done with the thread
		boost::unique_lock<boost::shared_mutex> work_lock(work->mutex);
		work->thread_id = boost::thread::id();
	}

	return result;
}

background_worker::work* background_worker::find_work(work_id id) const {
	auto it = std::find_if(_works.begin(), _works.end(),
		[&id](const auto& val) {
			return val.id == id;
		});

	if (it != _works.end()) {
		// We don't change this class instance, we just want to obtain non-const object from const_iterator
		return const_cast<work*>(&(*it));
	}

	return nullptr;
}

void background_worker::interruption_point() const {
	boost::this_thread::interruption_point();
}

void background_worker::report_progress(work_id id, int progress) {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	work* work = find_work(id);
	if (work) {
		boost::unique_lock<boost::shared_mutex> lock2(work->mutex);
		work->progress = progress;
		work->progress_changed = true;
	}
}

bool background_worker::is_busy_any() const {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	for (const auto& work : _works) {
		if (!work.future.is_ready()) {
			return true;
		}
	}		
	return false;
}

bool background_worker::is_busy(work_id id) const {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	work* work = find_work(id);
	return work && !work->future.is_ready();
}

void background_worker::cancel_all() {
	// work::cancelled will be set in launch_work() after interruption
	_thread_pool.interrupt_all();
}

void background_worker::cancel(work_id id) {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	work* work = find_work(id);
	if (work && !work->future.is_ready()) {
		boost::upgrade_lock<boost::shared_mutex> work_lock(work->mutex);
		if (!work->cancelled) {
			bool interrupted = false;
			if (work->thread_id != boost::thread::id()) {
				auto thread = _thread_map.find(work->thread_id);
				if (thread != _thread_map.end()) {
					// work::cancelled will be set in launch_work() after interruption
					(*thread).second->interrupt();
					interrupted = true;
				}
			}
			if (!interrupted) {
				boost::upgrade_to_unique_lock<boost::shared_mutex> upgrade(work_lock);
				work->cancelled = true;
			}
		}
	}
}

bool background_worker::is_cancelled(work_id id) const {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	work* work = find_work(id);
	if (work) {
		boost::shared_lock<boost::shared_mutex> lock2(work->mutex);
		return work->cancelled;
	}
	return false;
}

void background_worker::wait_all() const {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	for (const auto& work : _works) {
		work.future.wait();
	}
}

void background_worker::wait(work_id id) const {
	boost::shared_lock<boost::shared_mutex> lock(_mutex);
	const work* work = find_work(id);
	if (work) {
		work->future.wait();
	}
}

void background_worker::initialize() {
	BOOST_ASSERT_MSG(!_thread_pool.size(), "initialize() has already been called");

	if (_thread_pool.size()) {
		BOOST_THROW_EXCEPTION(std::runtime_error("initialize() has already been called"));
	}

	if (!_number_of_threads) {
		_number_of_threads = boost::thread::hardware_concurrency();
		if (!_number_of_threads) {
			_number_of_threads = 1;
		}
	}

	for (size_t i = 0; i < _number_of_threads; ++i) {
		boost::thread* thread = _thread_pool.create_thread(boost::bind(&boost::asio::io_service::run, &_io));
		_thread_map.insert(std::make_pair(thread->get_id(), thread));
	}
}

void background_worker::update() {
	// In general we only read collection
	boost::upgrade_lock<boost::shared_mutex> lock(_mutex);

	for (auto it = _works.begin(); it != _works.end(); /**/) {
		work& work = *it;
		bool run = false;

		boost::upgrade_lock<boost::shared_mutex> lock_work(work.mutex);
		// Call progress callback if progress is changed
		if (work.progress_changed) {
			if (work.callback && !work.cancelled) {
				run = true;
			}
			boost::upgrade_to_unique_lock<boost::shared_mutex> upgrade(lock_work);
			work.progress_changed = false;
		}

		// Run it under upgrade lock, not unique
		if (run) {
			try {
				work.callback->on_progress_changed((*it).id, this, work.progress);
			} catch (...) {
				BOOST_ASSERT_MSG(false, "this method is not supposed to throw any exceptions");
			}
		}

		if (work.future.is_ready()) {
			// Call post execute callback and erase the item
			try {
				async_result param(work.cancelled, work.future);
				if (work.callback) {
					work.callback->on_post_execute((*it).id, this, param);
				} else if (work.result) {
					work.result((*it).id, param);
				}
			} catch (...) {
				BOOST_ASSERT_MSG(false, "this method is not supposed to throw any exceptions");
			}
			// We should unlock work's mutex now, because we're going to delete the work
			lock_work.unlock();
			// Propagate upgrade to unique lock so as to modify collection
			boost::upgrade_to_unique_lock<boost::shared_mutex> upgrade(lock);
			_works.erase(it++);
		} else {
			// If future isn't ready, skip it for the present
			++it;
		}
	}
}
