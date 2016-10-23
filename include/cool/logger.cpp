#include "pch.h"
#include "cool/logger.hpp"
#include "cool/platform.hpp"
#include "cool/io.hpp"

#include <ctime>

#include <boost/thread.hpp>

#ifdef TARGET_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#ifdef TARGET_PLATFORM_ANDROID
#include <android/log.h>
#endif

typedef std::vector<logger_sink_ptr> sinks_type;

// TODO: do something with statics
static sinks_type s_sinks;
static boost::mutex s_mutex;

logger::logger(priority level)
	: _level(level)
{
}

logger::~logger() {
	try {
		_stream << std::endl;
		std::string msg = _stream.str();

		boost::mutex::scoped_lock lock(s_mutex);
		std::for_each(s_sinks.begin(), s_sinks.end(), [&](logger_sink_ptr sink) {
			sink->write(_level, msg);
		});
	} catch (...) {
		// don't let an exception come out from destructor
	}
}

void logger::add_sink(logger_sink* sink) {
	boost::mutex::scoped_lock lock(s_mutex);
	s_sinks.push_back(sink);
}

void logger::clear_sinks() {
	boost::mutex::scoped_lock lock(s_mutex);
	s_sinks.clear();
}

/// debug output sink
class debug_output_logger_sink : public logger_sink {
public:
	void write(priority level, const std::string& msg) {
#if defined(TARGET_PLATFORM_WINDOWS)
		const char* tag = (
			level == priority::debug ? "[d]" :
			level == priority::info  ? "[i]" :
			level == priority::warn  ? "[*]" :
			level == priority::error ? "[!]" : nullptr);
	
		if (tag) {
			::OutputDebugStringA(tag);
		}

		char buffer[32];
		std::time_t time = std::time(nullptr);
		strftime(buffer, sizeof(buffer), "[%X] ", std::localtime(&time));
	
		::OutputDebugStringA(buffer);
		::OutputDebugStringA(msg.c_str());
#elif defined(TARGET_PLATFORM_ANDROID)
		int Priority =
			level == priority::debug ? ANDROID_LOG_DEBUG :
			level == priority::info ? ANDROID_LOG_INFO :
			level == priority::warn ? ANDROID_LOG_WARN :
			level == priority::error ? ANDROID_LOG_ERROR : ANDROID_LOG_VERBOSE;
	
		__android_log_write(Priority, "cool", msg.c_str());
#endif
	}
};

/// stream sink
class stream_logger_sink : public logger_sink {
public:
	stream_logger_sink(io::output_stream* stream)
		: _stream(stream)
	{
	}

	void write(priority level, const std::string& msg) {
		if (_stream) {
			const char* tag = (
				level == priority::debug ? "[D]" :
				level == priority::info  ? "[i]" :
				level == priority::warn  ? "[*]" :
				level == priority::error ? "[!]" : "   ");
		
		
			_stream->write(tag, 3);
		
			char buffer[32];
			std::time_t time = std::time(nullptr);
			strftime(buffer, sizeof(buffer), "[%X] ", std::localtime(&time));

			_stream->write(buffer, std::strlen(buffer));
			_stream->write(msg.c_str(), msg.size());
			_stream->flush();
		}
	}

private:
	io::output_stream_ptr _stream;
};

logger_sink* logger_sink::create_debug_output_sink() {
	return new debug_output_logger_sink();
}

logger_sink* logger_sink::create_stream_sink(io::output_stream* stream) {
	return new stream_logger_sink(stream);
}
