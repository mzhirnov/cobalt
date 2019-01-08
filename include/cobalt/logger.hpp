#pragma once

// Classes in this file:
//     logger
//     logger_sink

#include "cool/common.hpp"
#include "cool/enum_traits.hpp"

#include <sstream>

#define LOGGING_LEVEL_NONE 0
#define LOGGING_LEVEL_ERROR 1
#define LOGGING_LEVEL_WARN 2
#define LOGGING_LEVEL_INFO 3
#define LOGGING_LEVEL_DEBUG 4

#ifndef LOGGING_LEVEL
#if defined(_DEBUG) || !defined(NDEBUG)
#define LOGGING_LEVEL LOGGING_LEVEL_DEBUG
#else
#define LOGGING_LEVEL LOGGING_LEVEL_INFO
#endif
#endif // LOGGING_LEVEL

#define LOGD(msg) if (LOGGING_LEVEL >= LOGGING_LEVEL_DEBUG) logger(priority::debug) << msg
#define LOGI(msg) if (LOGGING_LEVEL >= LOGGING_LEVEL_INFO) logger(priority::info) << msg
#define LOGW(msg) if (LOGGING_LEVEL >= LOGGING_LEVEL_WARN) logger(priority::warn) << msg
#define LOGE(msg) if (LOGGING_LEVEL >= LOGGING_LEVEL_ERROR) logger(priority::error) << msg

class logger_sink;

DEFINE_ENUM_CLASS(
	priority, uint32_t,
	debug,
	info,
	warn,
	error
)

/// logger
class logger {
public:
	logger(priority level);
	~logger();

	template <typename T>
	std::ostringstream& operator << (const T& t) {
		_stream << t;
		return _stream;
	}

	static void add_sink(logger_sink* sink);
	static void clear_sinks();

private:
	std::ostringstream _stream;
	priority _level;
};

namespace io {
	class output_stream;
}

/// logger sink
class logger_sink : public ref_counter<logger_sink> {
public:
	virtual ~logger_sink() {}
	virtual void write(priority level, const std::string& msg) = 0;

	static logger_sink* create_debug_output_sink();
	static logger_sink* create_stream_sink(io::output_stream* stream);
};

typedef boost::intrusive_ptr<logger_sink> logger_sink_ptr;
