// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/modules/stack_unwinder/public/module.h"

#include <dlfcn.h>

#include "base/android/bundle_utils.h"
#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "chrome/android/modules/stack_unwinder/provider/jni_headers/StackUnwinderModuleProvider_jni.h"

namespace stack_unwinder {

namespace {

void* TryLoadModule() {
  const char* chrome_target_possibilities[] = {
      "monochrome",
      "chrome",
  };

  const char partition_name[] = "stack_unwinder_partition";
  for (const char* target : chrome_target_possibilities) {
    void* module = base::android::BundleUtils::DlOpenModuleLibraryPartition(
        base::StrCat({target, "_", partition_name}), partition_name);
    if (module)
      return module;
  }

  return nullptr;
}

}  // namespace

// static
bool Module::IsInstalled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_StackUnwinderModuleProvider_isModuleInstalled(env);
}

// static
void Module::RequestInstallation() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_StackUnwinderModuleProvider_installModule(env);
}

// static
std::unique_ptr<Module> Module::TryLoad() {
  void* module = TryLoadModule();
  if (!module)
    return nullptr;

  CreateMemoryRegionsMapFunction create_memory_regions_map =
      reinterpret_cast<CreateMemoryRegionsMapFunction>(
          dlsym(module, "CreateMemoryRegionsMap"));
  CHECK(create_memory_regions_map);

  CreateNativeUnwinderFunction create_native_unwinder =
      reinterpret_cast<CreateNativeUnwinderFunction>(
          dlsym(module, "CreateNativeUnwinder"));
  CHECK(create_native_unwinder);

  return base::WrapUnique(
      new Module(create_memory_regions_map, create_native_unwinder));
}

std::unique_ptr<MemoryRegionsMap> Module::CreateMemoryRegionsMap() {
  return base::WrapUnique(create_memory_regions_map_());
}

std::unique_ptr<base::Unwinder> Module::CreateNativeUnwinder(
    MemoryRegionsMap* memory_regions_map) {
  return base::WrapUnique(create_native_unwinder_(memory_regions_map));
}

Module::Module(CreateMemoryRegionsMapFunction create_memory_regions_map,
               CreateNativeUnwinderFunction create_native_unwinder)
    : create_memory_regions_map_(create_memory_regions_map),
      create_native_unwinder_(create_native_unwinder) {}

}  // namespace stack_unwinder
