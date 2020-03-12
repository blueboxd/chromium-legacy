// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/android/tile_provider_bridge.h"

#include "chrome/browser/upboarding/query_tiles/jni_headers/TileProviderBridge_jni.h"

namespace upboarding {

TileProviderBridge::TileProviderBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_TileProviderBridge_create(env, reinterpret_cast<int64_t>(this))
               .obj());
}

TileProviderBridge::~TileProviderBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_TileProviderBridge_clearNativePtr(env, java_obj_);
}

void TileProviderBridge::GetQueryTiles(JNIEnv* env,
                                       const JavaParamRef<jobject>& jcaller,
                                       const JavaParamRef<jobject>& jcallback) {
}

void TileProviderBridge::GetThumbnail(JNIEnv* env,
                                      const JavaParamRef<jobject>& jcaller,
                                      const JavaParamRef<jstring>& jid,
                                      const JavaParamRef<jobject>& jcallback) {}

}  // namespace upboarding
