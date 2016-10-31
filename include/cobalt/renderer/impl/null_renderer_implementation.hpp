#pragma once

#include <iostream>
#include <sstream>
#include <thread>

#include <boost/smart_ptr/intrusive_ref_counter.hpp>

inline void print(const char* message) {
	std::ostringstream oss;
	oss << std::this_thread::get_id() << " " << message << std::endl;
	std::cout << oss.str();
}

class null_renderer_implementation
	: public boost::intrusive_ref_counter<null_renderer_implementation, boost::thread_unsafe_counter>
{
public:
	null_renderer_implementation();	
	~null_renderer_implementation();

	void initialize(window_handle_type);

	void shutdown();

	void begin_scene();
	
	//void draw(const geometry* g, const material* m);
	
	void end_scene();
};

inline null_renderer_implementation::null_renderer_implementation() {
	print(__FUNCTION__);
}

inline null_renderer_implementation::~null_renderer_implementation() {
	print(__FUNCTION__);
}

inline void null_renderer_implementation::initialize(window_handle_type) {
	print(__FUNCTION__);
}

inline void null_renderer_implementation::shutdown() {
	print(__FUNCTION__);
}

inline void null_renderer_implementation::begin_scene() {
	print(__FUNCTION__);
	//BOOST_THROW_EXCEPTION(std::runtime_error("begin_scene failed"));
}

//inline void null_renderer_implementation::draw(const geometry* g, const material* m) {
//	print(__FUNCTION__);
//}

inline void null_renderer_implementation::end_scene() {
	print(__FUNCTION__);
	//BOOST_THROW_EXCEPTION(std::runtime_error("end_scene failed"));
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
}
