// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/dynamicmodule/module_metrics.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "chrome/android/chrome_jni_headers/ModuleMetrics_jni.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace customtabs {

namespace {

const char* kDexCacheName = "[anon:.bss]";
const char* kFileType[] = {".dex", ".odex", ".vdex", ".oat"};

}  // namespace

bool IsValidFileType(const std::string& file_name) {
  base::FilePath file_path(file_name);

  for (const char* file_type : kFileType) {
    if (file_path.MatchesExtension(file_type))
      return true;
  }
  return false;
}

void GetCodeMemoryFootprint(const std::string& package_name,
                            uint64_t* proportional_set_size_kb,
                            uint64_t* resident_set_size_kb) {
  bool matched = false;

  std::vector<memory_instrumentation::mojom::VmRegionPtr> maps =
      memory_instrumentation::OSMetrics::GetProcessMemoryMaps(
          base::GetCurrentProcId());

  for (const auto& region : maps) {
    if (IsValidFileType(region->mapped_file) &&
        region->mapped_file.find(package_name) != std::string::npos) {
      matched = true;
      *proportional_set_size_kb += region->byte_stats_proportional_resident;

      *resident_set_size_kb += (region->byte_stats_private_dirty_resident +
                                region->byte_stats_private_clean_resident +
                                region->byte_stats_shared_dirty_resident +
                                region->byte_stats_shared_clean_resident);

    } else {
      // The first [anon:bss] mapping that immediately follows a matched line
      // is the “dex cache”
      if (matched && !region->mapped_file.compare(kDexCacheName)) {
        *proportional_set_size_kb += region->byte_stats_proportional_resident;

        *resident_set_size_kb += (region->byte_stats_private_dirty_resident +
                                  region->byte_stats_private_clean_resident +
                                  region->byte_stats_shared_dirty_resident +
                                  region->byte_stats_shared_clean_resident);
      }

      matched = false;
    }
  }

  *proportional_set_size_kb /= 1024;
  *resident_set_size_kb /= 1024;
}

void RecordCodeMemoryFootprint(const std::string& package_name,
                               const std::string& suffix) {
  uint64_t proportional_set_size_kb = 0;
  uint64_t resident_set_size_kb = 0;

  GetCodeMemoryFootprint(package_name, &proportional_set_size_kb,
                         &resident_set_size_kb);

  // Not using macros since we don't need caching.
  std::string name = "CustomTabs.DynamicModule.ProportionalSet." + suffix;
  base::UmaHistogramCounts100000(name, proportional_set_size_kb);
  name = "CustomTabs.DynamicModule.ResidentSet." + suffix;
  base::UmaHistogramCounts100000(name, resident_set_size_kb);
}

static void JNI_ModuleMetrics_RecordCodeMemoryFootprint(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jpackage_name,
    const base::android::JavaParamRef<jstring>& jsuffix) {
  std::string package_name =
      base::android::ConvertJavaStringToUTF8(env, jpackage_name);
  std::string suffix = base::android::ConvertJavaStringToUTF8(env, jsuffix);

  RecordCodeMemoryFootprint(package_name, suffix);
}

}  // namespace customtabs
