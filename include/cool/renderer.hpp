#pragma once

#include "renderer/basic_renderer.h"
#include "renderer/basic_renderer_service.h"
#include "renderer/async_renderer_service.h"
#include "renderer/impl/null_renderer_implementation.h"
#include "renderer/impl/gles_renderer_implementation.h"

typedef basic_renderer<basic_renderer_service<null_renderer_implementation>> null_renderer;
typedef basic_renderer<async_renderer_service<null_renderer_implementation>> async_null_renderer;
typedef basic_renderer<basic_renderer_service<gles_renderer_implementation>> gles_renderer;
typedef basic_renderer<async_renderer_service<gles_renderer_implementation>> async_gles_renderer;
