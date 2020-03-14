// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/paint_preview/services/paint_preview_demo_service_factory.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "chrome/browser/paint_preview/services/paint_preview_demo_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"
#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/android/chrome_jni_headers/PaintPreviewDemoServiceFactory_jni.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif  // defined(OS_ANDROID)

namespace paint_preview {

#if defined(OS_ANDROID)
base::android::ScopedJavaLocalRef<jobject>
JNI_PaintPreviewDemoServiceFactory_GetServiceInstanceForCurrentProfile(
    JNIEnv* env) {
  ProfileKey* profile_key =
      ProfileManager::GetLastUsedProfile()->GetProfileKey();
  base::android::ScopedJavaGlobalRef<jobject> java_ref =
      PaintPreviewDemoServiceFactory::GetForKey(profile_key)->GetJavaRef();
  return base::android::ScopedJavaLocalRef<jobject>(java_ref);
}
#endif  // defined(OS_ANDROID)

// static
PaintPreviewDemoServiceFactory* PaintPreviewDemoServiceFactory::GetInstance() {
  return base::Singleton<PaintPreviewDemoServiceFactory>::get();
}

// static
PaintPreviewDemoService* PaintPreviewDemoServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<PaintPreviewDemoService*>(
      GetInstance()->GetServiceForKey(key, true));
}

PaintPreviewDemoServiceFactory::PaintPreviewDemoServiceFactory()
    : SimpleKeyedServiceFactory("paint_preview::PaintPreviewDemoService",
                                SimpleDependencyManager::GetInstance()) {}
PaintPreviewDemoServiceFactory::~PaintPreviewDemoServiceFactory() = default;

std::unique_ptr<KeyedService>
PaintPreviewDemoServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  if (key->IsOffTheRecord())
    return nullptr;

  return std::make_unique<PaintPreviewDemoService>(key->GetPath(),
                                                   key->IsOffTheRecord());
}

SimpleFactoryKey* PaintPreviewDemoServiceFactory::GetKeyToUse(
    SimpleFactoryKey* key) const {
  return key;
}

}  // namespace paint_preview
