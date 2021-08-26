/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "hwc-platform-drm-stm32mpu"

#include "platformstm32mpu.h"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <log/log.h>

#include <vivante_gralloc_handle.h>
#include <hardware/gralloc.h>

namespace android {

#ifdef USE_STM32MPU_IMPORTER
// static
Importer *Importer::CreateInstance(DrmDevice *drm) {
  DrmStm32mpuImporter *importer = new DrmStm32mpuImporter(drm);
  if (!importer)
    return NULL;

  int ret = importer->Init();
  if (ret) {
    ALOGE("Failed to initialize the STM32MPU importer %d", ret);
    delete importer;
    return NULL;
  }
  return importer;
}
#endif

uint32_t DrmStm32mpuImporter::ConvertHalFormatToDrm(uint32_t hal_format) {
  switch (hal_format) {
    case HAL_PIXEL_FORMAT_RGB_888:
      return DRM_FORMAT_RGB888;
    case HAL_PIXEL_FORMAT_BGRA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGBX_8888:
      return DRM_FORMAT_XRGB8888;
    case HAL_PIXEL_FORMAT_RGBA_8888:
      return DRM_FORMAT_ARGB8888;
    case HAL_PIXEL_FORMAT_RGB_565:
      return DRM_FORMAT_RGB565;
    case HAL_PIXEL_FORMAT_YV12:
      return DRM_FORMAT_YVU420;
    default:
      ALOGE("Cannot convert hal format to drm format %u", hal_format);
      return -EINVAL;
  }
}

uint32_t DrmStm32mpuImporter::HalFormatToBitsPerPixel(uint32_t hal_format)
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

int DrmStm32mpuImporter::ConvertBoInfo(buffer_handle_t handle, hwc_drm_bo_t *bo) {

  private_handle_t *gr_handle = (private_handle_t*)handle;
  if (!gr_handle)
    return -EINVAL;

  memset(bo, 0, sizeof(hwc_drm_bo_t));

  /* Extra bits are responsible for buffer compression and memory layout */
  if (gr_handle->format & ~0x10f) {
    ALOGV("Special buffer formats are not supported");
    return -EINVAL;
  }

  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->format = ConvertHalFormatToDrm(gr_handle->format);

  uint32_t bpp = HalFormatToBitsPerPixel(gr_handle->format);
  bo->pitches[0] = gr_handle->stride * bpp;

  bo->prime_fds[0] = gr_handle->fd;
  bo->offsets[0] = 0;

  return 0;
}

#ifdef USE_STM32MPU_IMPORTER
std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageGreedy>();
  return planner;
}
#endif
}
