/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "hwc-bufferinfo-stm32mpu"

#include "BufferInfoStm32mpu.h"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <log/log.h>

#include <vivante_gralloc_handle.h>
#include <hardware/gralloc.h>

namespace android {

LEGACY_BUFFER_INFO_GETTER(BufferInfoStm32mpu);

uint32_t BufferInfoStm32mpu::ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_BGR888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XBGR8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ABGR8888;
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_BGR565;
    case HAL_PIXEL_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
    default:
      ALOGE("Cannot convert hal format to drm format %u", hal_format);
      return -EINVAL;
  }
}

uint32_t BufferInfoStm32mpu::HalFormatToBitsPerPixel(uint32_t hal_format)
{
    uint32_t bpp;

    switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        bpp = 4;
        break;
    case HAL_PIXEL_FORMAT_RGB_888:
        bpp = 3;
        break;
    case HAL_PIXEL_FORMAT_RGB_565:
        bpp = 2;
        break;
    case HAL_PIXEL_FORMAT_YV12:
        bpp = 1;
        break;
    default:
        bpp = 0;
        break;
    }

    return bpp;
}

auto BufferInfoStm32mpu::GetBoInfo(buffer_handle_t handle)
    -> std::optional<BufferInfo> {

  private_handle_t *gr_handle = (private_handle_t*)handle;
  if (!gr_handle)
    return {};

  BufferInfo bi{};

  /* Extra bits are responsible for buffer compression and memory layout */
  if (gr_handle->format & ~0x10f) {
    ALOGV("Special buffer formats are not supported");
    return {};
  }

  bi.width = gr_handle->width;
  bi.height = gr_handle->height;
  bi.format = ConvertHalFormatToDrm(gr_handle->format);

  uint32_t bpp = HalFormatToBitsPerPixel(gr_handle->format);
  bi.pitches[0] = gr_handle->stride * bpp;

  bi.prime_fds[0] = gr_handle->fd;
  bi.offsets[0] = 0;

  return bi;
}

}  // namespace android
