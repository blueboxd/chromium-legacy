// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/video_tutorials/internal/android/video_tutorial_service_bridge.h"

#include <memory>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "chrome/browser/video_tutorials/jni_headers/VideoTutorialServiceBridge_jni.h"

using base::android::AttachCurrentThread;

namespace video_tutorials {
namespace {

const char kVideoTutorialServiceBridgeKey[] = "video_tutorial_service_bridge";

}  // namespace

// static
ScopedJavaLocalRef<jobject>
VideoTutorialServiceBridge::GetBridgeForVideoTutorialService(
    VideoTutorialService* video_tutorial_service) {
  if (!video_tutorial_service->GetUserData(kVideoTutorialServiceBridgeKey)) {
    video_tutorial_service->SetUserData(
        kVideoTutorialServiceBridgeKey,
        std::make_unique<VideoTutorialServiceBridge>(video_tutorial_service));
  }

  VideoTutorialServiceBridge* bridge = static_cast<VideoTutorialServiceBridge*>(
      video_tutorial_service->GetUserData(kVideoTutorialServiceBridgeKey));

  return ScopedJavaLocalRef<jobject>(bridge->java_obj_);
}

VideoTutorialServiceBridge::VideoTutorialServiceBridge(
    VideoTutorialService* video_tutorial_service)
    : video_tutorial_service_(video_tutorial_service) {
  DCHECK(video_tutorial_service_);
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(env, Java_VideoTutorialServiceBridge_create(
                           env, reinterpret_cast<int64_t>(this))
                           .obj());
}

VideoTutorialServiceBridge::~VideoTutorialServiceBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_VideoTutorialServiceBridge_clearNativePtr(env, java_obj_);
}

}  // namespace video_tutorials
