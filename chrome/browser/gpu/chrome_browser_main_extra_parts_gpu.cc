// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu/chrome_browser_main_extra_parts_gpu.h"

#include "base/logging.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/viz/common/features.h"
#include "content/public/browser/gpu_data_manager.h"
#include "gpu/config/gpu_info.h"

namespace {
const char kTrialName[] = "SkiaBackend";
const char kGL[] = "GL";
const char kVulkan[] = "Vulkan";
}  // namespace

ChromeBrowserMainExtraPartsGpu::ChromeBrowserMainExtraPartsGpu() = default;

ChromeBrowserMainExtraPartsGpu::~ChromeBrowserMainExtraPartsGpu() {
  if (features::IsUsingSkiaRenderer())
    content::GpuDataManager::GetInstance()->RemoveObserver(this);
}

void ChromeBrowserMainExtraPartsGpu::PostEarlyInitialization() {
  if (features::IsUsingSkiaRenderer())
    content::GpuDataManager::GetInstance()->AddObserver(this);
}

void ChromeBrowserMainExtraPartsGpu::OnGpuInfoUpdate() {
  DCHECK(features::IsUsingSkiaRenderer());
  const auto* backend_name = GetSkiaBackendName();
  if (backend_name) {
    ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
        kTrialName, GetSkiaBackendName());
  }
}

const char* ChromeBrowserMainExtraPartsGpu::GetSkiaBackendName() const {
  auto* manager = content::GpuDataManager::GetInstance();
  if (!manager->IsEssentialGpuInfoAvailable())
    return nullptr;
  if (manager->GetFeatureStatus(gpu::GpuFeatureType::GPU_FEATURE_TYPE_VULKAN) ==
      gpu::GpuFeatureStatus::kGpuFeatureStatusEnabled) {
    return kVulkan;
  }
  return kGL;
}
