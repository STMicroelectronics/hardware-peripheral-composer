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

uint32_t DrmStm32mpuImporter::ConvertDrmFormatToHal(uint32_t drm_format) {
  switch (drm_format) {
    case DRM_FORMAT_RGB888:
      return HAL_PIXEL_FORMAT_RGB_888;
    case DRM_FORMAT_ARGB8888:
      return HAL_PIXEL_FORMAT_BGRA_8888;
    case DRM_FORMAT_XRGB8888:
      return HAL_PIXEL_FORMAT_RGBX_8888;
    case DRM_FORMAT_ABGR8888:
      return HAL_PIXEL_FORMAT_RGBA_8888;
    case DRM_FORMAT_RGB565:
      return HAL_PIXEL_FORMAT_RGB_565;
    case DRM_FORMAT_YVU420:
      return HAL_PIXEL_FORMAT_YV12;
    default:
      ALOGE("Cannot convert drm format to hal format %u", drm_format);
      return -EINVAL;
  }
}

uint32_t DrmStm32mpuImporter::GetBpp(uint32_t hal_format)
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

// check usage validity (HW_FB or no SW read/write access)
static bool isUsageValid(int usage) {
  return ((usage & GRALLOC_USAGE_HW_FB) == GRALLOC_USAGE_HW_FB) ||
  (((usage & GRALLOC_USAGE_SW_READ_MASK) == GRALLOC_USAGE_SW_READ_NEVER) &&
  ((usage & GRALLOC_USAGE_SW_WRITE_MASK) == GRALLOC_USAGE_SW_WRITE_NEVER));
}

int DrmStm32mpuImporter::ConvertBoInfo(buffer_handle_t handle, hwc_drm_bo_t *bo) {

  private_handle_t *gr_handle = (private_handle_t*)handle;
  if (!gr_handle)
    return -EINVAL;

  memset(bo, 0, sizeof(hwc_drm_bo_t));

  if (! isUsageValid(gr_handle->usage))
    return -EPERM;

  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->format = ConvertHalFormatToDrm(gr_handle->format);

  uint32_t bpp = GetBpp(gr_handle->format);
  bo->pitches[0] = gr_handle->stride * bpp;

  bo->prime_fds[0] = gr_handle->fd;
  bo->offsets[0] = 0;

  return 0;
}

class PlanStageStm32mpu : public Planner::PlanStage {
  public:
  int ProvisionPlanes(std::vector<DrmCompositionPlane> *composition,
                      std::map<size_t, DrmHwcLayer *> &layers, DrmCrtc *crtc,
                      std::vector<DrmPlane *> *planes) {
    int layers_added = 0;
    // Fill up the remaining planes
    for (auto i = layers.begin(); i != layers.end(); i = layers.erase(i)) {
      if (! isUsageValid(i->second->gralloc_buffer_usage))
        continue;

        int ret = Emplace(composition, planes, DrmCompositionPlane::Type::kLayer,
                        crtc, std::make_pair(i->first, i->second));
        layers_added++;
      // We don't have any planes left
      if (ret == -ENOENT)
        break;
      else if (ret)
        ALOGE("Failed to emplace layer %zu, dropping it", i->first);
    }
    // If we didn't emplace anything, return an error to ensure we force client
    // compositing.
    if (!layers_added)
      return -EINVAL;
    return 0;
  }
};

#ifdef USE_STM32MPU_IMPORTER
std::unique_ptr<Planner> Planner::CreateInstance(DrmDevice *) {
  std::unique_ptr<Planner> planner(new Planner);
  planner->AddStage<PlanStageStm32mpu>();
  return planner;
}
#endif
}
