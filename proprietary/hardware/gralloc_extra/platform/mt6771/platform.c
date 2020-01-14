#include "../ge_platform.h"

#define LOG_TAG "GE"
#include <cutils/log.h>

#include <system/graphics.h>
#include <graphics_mtk_defs.h>
#include <gralloc1_mtk_defs.h>

static int _plt_gralloc_extra_get_platform_format(int in_format, uint64_t usage)
{
	if (in_format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
	{
		int format = HAL_PIXEL_FORMAT_RGBA_8888;

		const uint64_t u_video = GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER;
		const uint64_t u_camera = GRALLOC1_PRODUCER_USAGE_CAMERA;
		const uint64_t u_camera_texture = GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE | GRALLOC1_PRODUCER_USAGE_CAMERA;
		const uint64_t u_camera_composer = GRALLOC1_CONSUMER_USAGE_HWCOMPOSER | GRALLOC1_PRODUCER_USAGE_CAMERA;

		if ((usage & u_video) == u_video)
			format = HAL_PIXEL_FORMAT_YV12;
		else if ((usage & u_camera_texture) == u_camera_texture)
			format = HAL_PIXEL_FORMAT_YV12;
		else if ((usage & u_camera_composer) == u_camera_composer)
			format = HAL_PIXEL_FORMAT_YV12;
		else if ((usage & u_camera) == u_camera)
			format = HAL_PIXEL_FORMAT_YV12;

		return format;
	}

	if (in_format == HAL_PIXEL_FORMAT_YCbCr_420_888)
	{
		int format = HAL_PIXEL_FORMAT_YV12;

		const uint64_t u_video = GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER;
		const uint64_t u_camera = GRALLOC1_PRODUCER_USAGE_CAMERA;
		const uint64_t u_camera_texture = GRALLOC1_CONSUMER_USAGE_GPU_TEXTURE | GRALLOC1_PRODUCER_USAGE_CAMERA;
		const uint64_t u_camera_composer = GRALLOC1_CONSUMER_USAGE_HWCOMPOSER | GRALLOC1_PRODUCER_USAGE_CAMERA;

		if ((usage & u_camera_texture) == u_camera_texture)
			format = HAL_PIXEL_FORMAT_YV12;
		else if ((usage & u_camera_composer) == u_camera_composer)
			format = HAL_PIXEL_FORMAT_YV12;
		else if ((usage & u_camera) == u_camera)
			format = HAL_PIXEL_FORMAT_YV12;

		return format;
	}

	return in_format;
}

void ge_platform_wrap_init(ge_platform_fn *table)
{
	table->gralloc_extra_get_platform_format = _plt_gralloc_extra_get_platform_format;
}
