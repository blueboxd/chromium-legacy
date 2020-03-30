// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/android/tile_provider_bridge.h"

#include <memory>
#include <string>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/upboarding/query_tiles/jni_headers/TileProviderBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace upboarding {

namespace {
const char kTileProviderBridgeKey[] = "tile_provider_bridge";

ScopedJavaLocalRef<jobject> createJavaTileAndMaybeAddToList(
    JNIEnv* env,
    ScopedJavaLocalRef<jobject> jlist,
    QueryTileEntry* tile) {
  ScopedJavaLocalRef<jobject> jchildren =
      Java_TileProviderBridge_createList(env);

  for (const auto& subtile : tile->sub_tiles)
    createJavaTileAndMaybeAddToList(env, jchildren, subtile.get());

  return Java_TileProviderBridge_createTileAndMaybeAddToList(
      env, jlist, ConvertUTF8ToJavaString(env, tile->id),
      ConvertUTF8ToJavaString(env, tile->display_text),
      ConvertUTF8ToJavaString(env, tile->accessibility_text),
      ConvertUTF8ToJavaString(env, tile->query_text), jchildren);
}

ScopedJavaLocalRef<jobject> createJavaTiles(
    JNIEnv* env,
    const std::vector<QueryTileEntry*>& tiles) {
  ScopedJavaLocalRef<jobject> jlist = Java_TileProviderBridge_createList(env);

  for (QueryTileEntry* tile : tiles)
    createJavaTileAndMaybeAddToList(env, jlist, tile);

  return jlist;
}

void RunGetTilesCallback(const JavaRef<jobject>& j_callback,
                         const std::vector<QueryTileEntry*>& tiles) {
  JNIEnv* env = AttachCurrentThread();
  RunObjectCallbackAndroid(j_callback, createJavaTiles(env, tiles));
}

void RunGeVisualsCallback(const JavaRef<jobject>& j_callback,
                          const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap =
      gfx::ConvertToJavaBitmap(image.ToSkBitmap());
  RunObjectCallbackAndroid(j_callback, j_bitmap);
}

}  // namespace

// static
ScopedJavaLocalRef<jobject> TileProviderBridge::GetBridgeForTileService(
    TileService* tile_service) {
  if (!tile_service->GetUserData(kTileProviderBridgeKey)) {
    tile_service->SetUserData(
        kTileProviderBridgeKey,
        std::make_unique<TileProviderBridge>(tile_service));
  }

  TileProviderBridge* bridge = static_cast<TileProviderBridge*>(
      tile_service->GetUserData(kTileProviderBridgeKey));

  return ScopedJavaLocalRef<jobject>(bridge->java_obj_);
}

TileProviderBridge::TileProviderBridge(TileService* tile_service)
    : tile_service_(tile_service) {
  DCHECK(tile_service_);
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
  tile_service_->GetQueryTiles(base::BindOnce(
      &RunGetTilesCallback, ScopedJavaGlobalRef<jobject>(jcallback)));
}

void TileProviderBridge::GetVisuals(JNIEnv* env,
                                    const JavaParamRef<jobject>& jcaller,
                                    const JavaParamRef<jstring>& jid,
                                    const JavaParamRef<jobject>& jcallback) {
  std::string tile_id = ConvertJavaStringToUTF8(env, jid);
  tile_service_->GetVisuals(
      tile_id, base::BindOnce(&RunGeVisualsCallback,
                              ScopedJavaGlobalRef<jobject>(jcallback)));
}

}  // namespace upboarding
