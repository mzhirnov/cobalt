#pragma once

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <boost/predef.h>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/exception/all.hpp>

namespace detail {

class egl_context {
public:
	egl_context() = default;

	egl_context(const egl_context&) = delete;
	egl_context& operator=(const egl_context&) = delete;

	~egl_context() {
		shutdown();
	}

	void initialize(window_handle_type window) {
		if (_initialized) {
			return;
		}

		if (window != window_handle_type())
			_window = window;

		create_display();
		create_surface();
		create_context();

		_initialized = true;
	}

	void shutdown() {
		if (_display != EGL_NO_DISPLAY) {
			eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

			if (_context != EGL_NO_CONTEXT) {
				eglDestroyContext(_display, _context);
				_context = EGL_NO_CONTEXT;
			}

			if (_surface != EGL_NO_SURFACE) {
				eglDestroySurface(_display, _surface);
				_surface = EGL_NO_SURFACE;
			}

			eglTerminate(_display);
			_display = EGL_NO_DISPLAY;
		}

		_initialized = false;
	}

	void suspend() {
		if (_surface != EGL_NO_SURFACE) {
			eglDestroySurface(_display, _surface);
			_surface = EGL_NO_SURFACE;
		}
	}

	void resume(window_handle_type window = window_handle_type()) {
		if (!_initialized) {
			initialize(window);
			return;
		}

		if (_surface == EGL_NO_SURFACE) {
			create_surface();
		}

		if (!eglMakeCurrent(_display, _surface, _surface, _context)) {
			EGLint err = eglGetError();
			if (err != EGL_SUCCESS) {
				shutdown();
				initialize(window);
			}
		}
	}

	void make_current() {
		if (!eglMakeCurrent(_display, _surface, _surface, _context)) {
			EGLint err = eglGetError();
			if (err != EGL_SUCCESS) {
				shutdown();
				initialize(window_handle_type());
			}
		}
	}

	void swap_buffers() {
		if (!eglSwapBuffers(_display, _surface)) {
			EGLint err = eglGetError();
			if (err != EGL_SUCCESS) {
				shutdown();
				initialize(window_handle_type());
			}
			//if (err == EGL_BAD_SURFACE) {
			//	create_surface();
			//} else if (err == EGL_BAD_CONTEXT) {
			//	create_context();
			//} else if (err == EGL_CONTEXT_LOST) {
			//	shutdown();
			//	initialize(window_handle_type());
			//}
		}
	}

	int get_screen_width() const { return _screen_width; }

	int get_screen_height() const { return _screen_height; }

	bool is_initialized() const { return _initialized; }

private:
	void create_display() {
#ifdef BOOST_OS_WINDOWS
		_display = eglGetDisplay(::GetDC(_window));
#else
		_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
		if (_display == EGL_NO_DISPLAY) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to get EGL display"));
		}

		if (!eglInitialize(_display, NULL, NULL)) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to initialize EGL"));
		}

		EGLint num_configs = 0;
		if (!eglGetConfigs(_display, NULL, 0, &num_configs)) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to get EGL configs"));
		}

		if (!num_configs) {
			BOOST_THROW_EXCEPTION(std::runtime_error("there is no EGL configs"));
		}

		const EGLint attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
			EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24, EGL_NONE };

		eglChooseConfig(_display, attribs, &_config, 1, &num_configs);
		if (!num_configs) {
			const EGLint attribs2[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
				EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };

			eglChooseConfig(_display, attribs2, &_config, 1, &num_configs);
			if (!num_configs) {
				const EGLint attribs3[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
					EGL_RED_SIZE, 5, EGL_GREEN_SIZE, 6, EGL_BLUE_SIZE, 5, EGL_DEPTH_SIZE, 16, EGL_NONE };

				eglChooseConfig(_display, attribs3, &_config, 1, &num_configs);
				if (!num_configs) {
					BOOST_THROW_EXCEPTION(std::runtime_error("failed to choose EGL config"));
				}
			}
		}
	}

	void create_surface() {
		_surface = eglCreateWindowSurface(_display, _config, _window, NULL);
		if (_surface == EGL_NO_SURFACE) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to create EGL window surface"));
		}

		eglQuerySurface(_display, _surface, EGL_WIDTH, &_screen_width);
		eglQuerySurface(_display, _surface, EGL_HEIGHT, &_screen_height);
	}

	void create_context() {
		EGLint attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

		_context = eglCreateContext(_display, _config, EGL_NO_CONTEXT, attribs);
		if (_context == EGL_NO_CONTEXT) {
			BOOST_THROW_EXCEPTION(std::runtime_error("failed to create EGL context"));
		}

		make_current();
	}

private:
	window_handle_type _window = window_handle_type();

	EGLConfig _config = nullptr;
	EGLDisplay _display = EGL_NO_DISPLAY;
	EGLSurface _surface = EGL_NO_SURFACE;
	EGLContext _context = EGL_NO_CONTEXT;

	bool _initialized = false;

	int _screen_width = 0;
	int _screen_height = 0;
};

} // namespace detail

class gles_renderer_implementation
	: public boost::intrusive_ref_counter<gles_renderer_implementation, boost::thread_unsafe_counter>
{
public:
	void initialize(window_handle_type win) {
		_context.initialize(win);
	}

	void shutdown() {
		_context.shutdown();
	}

	void begin_scene() {
		_context.make_current();
	}

	//void draw(const geometry* g, const material* m);

	void end_scene() {
		_context.swap_buffers();
	}

private:
	detail::egl_context _context;
};
