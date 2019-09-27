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

#ifndef ANDROID_PLATFORM_DRM_STM32MPU_H_
#define ANDROID_PLATFORM_DRM_STM32MPU_H_

#include "drmdevice.h"
#include "platform.h"

#include <hardware/gralloc.h>

namespace android {

class DrmStm32mpuImporter : public Importer {
 public:
  DrmStm32mpuImporter(DrmDevice *drm);
  ~DrmStm32mpuImporter() override;

  int Init();

  int ImportBuffer(buffer_handle_t handle, hwc_drm_bo_t *bo) override;
  int ReleaseBuffer(hwc_drm_bo_t *bo) override;
  bool CanImportBuffer(buffer_handle_t handle) override;

 private:
  uint32_t ConvertHalFormatToDrm(uint32_t hal_format);
  uint32_t ConvertDrmFormatToHal(uint32_t drm_format);
  uint32_t GetBpp(uint32_t hal_format);

  DrmDevice *drm_;

  const gralloc_module_t *gralloc_;
};
}

#endif
