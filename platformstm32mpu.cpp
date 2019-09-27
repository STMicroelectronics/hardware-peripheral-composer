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
#include "drmdevice.h"
#include "platform.h"

#include <drm/drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <log/log.h>
#include <gralloc_handle.h>
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

DrmStm32mpuImporter::DrmStm32mpuImporter(DrmDevice *drm) : drm_(drm) {
}

DrmStm32mpuImporter::~DrmStm32mpuImporter() {
}

int DrmStm32mpuImporter::Init() {
  int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID,
                          (const hw_module_t **)&gralloc_);
  if (ret) {
    ALOGE("Failed to open gralloc module");
    return ret;
  }

  if (strcasecmp(gralloc_->common.name, "Vivante DRM Memory Allocator"))
    ALOGW("Using non-Vivante gralloc module: %s/%s\n", gralloc_->common.name,
          gralloc_->common.author);

  return 0;
}

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

int DrmStm32mpuImporter::ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) {

  private_handle_t *gr_handle = (private_handle_t*)handle;
  if (!gr_handle)
    return -EINVAL;

  memset(bo, 0, sizeof(hwc_drm_bo_t));

  if (! isUsageValid(gr_handle->usage))
    return -EPERM;

  uint32_t gem_handle;
  int ret = drmPrimeFDToHandle(drm_->fd(), gralloc_handle_fd(handle), &gem_handle);
  if (ret) {
    ALOGE("failed to import drm fd %d prime fd %d ret=%d", drm_->fd(), gralloc_handle_fd(handle), ret);
    return ret;
  }

  bo->width = gr_handle->width;
  bo->height = gr_handle->height;
  bo->format = ConvertHalFormatToDrm(gr_handle->format);

  uint32_t bpp = GetBpp(gr_handle->format);
  bo->pitches[0] = gr_handle->stride * bpp;

  bo->gem_handles[0] = gem_handle;
  bo->offsets[0] = 0;

  ret = drmModeAddFB2(drm_->fd(), bo->width, bo->height, bo->format,
                      bo->gem_handles, bo->pitches, bo->offsets, &bo->fb_id, 0);
  if (ret) {
    ALOGE("could not create drm fb %d", ret);
    return ret;
  }

  return ret;
}

int DrmStm32mpuImporter::ReleaseBuffer(hwc_drm_bo_t *bo) {
  if (bo->fb_id)
    if (drmModeRmFB(drm_->fd(), bo->fb_id))
      ALOGE("Failed to rm fb");

  struct drm_gem_close gem_close;
  memset(&gem_close, 0, sizeof(gem_close));
  int num_gem_handles = sizeof(bo->gem_handles) / sizeof(bo->gem_handles[0]);
  for (int i = 0; i < num_gem_handles; i++) {
    if (!bo->gem_handles[i])
      continue;

    gem_close.handle = bo->gem_handles[i];
    int ret = drmIoctl(drm_->fd(), DRM_IOCTL_GEM_CLOSE, &gem_close);
    if (ret)
      ALOGE("Failed to close gem handle %d %d with usage 0x%08x", i, ret, bo->usage);
    else {
      /* Clear any duplicate gem handle as well but don't close again */
      for (int j = i + 1; j < num_gem_handles; j++)
        if (bo->gem_handles[j] == bo->gem_handles[i])
          bo->gem_handles[j] = 0;
      bo->gem_handles[i] = 0;
    }
  }
  return 0;
}

bool DrmStm32mpuImporter::CanImportBuffer(buffer_handle_t handle) {
  if (handle == NULL)
    return false;
  return true;
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
