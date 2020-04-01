// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_stream_surface.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "chrome/android/chrome_jni_headers/FeedStreamSurface_jni.h"
#include "components/feed/core/proto/v2/ui.pb.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace feed {

static jlong JNI_FeedStreamSurface_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& j_this) {
  return reinterpret_cast<intptr_t>(new FeedStreamSurface(j_this));
}

FeedStreamSurface::FeedStreamSurface(const JavaRef<jobject>& j_this) {
  java_ref_.Reset(j_this);
}

FeedStreamSurface::~FeedStreamSurface() {}

void FeedStreamSurface::OnStreamUpdated(
    const feedui::StreamUpdate& stream_update) {
  JNIEnv* env = base::android::AttachCurrentThread();
  int32_t data_size = stream_update.ByteSize();
  std::vector<uint8_t> data;
  data.resize(data_size);
  stream_update.SerializeToArray(data.data(), data_size);
  ScopedJavaLocalRef<jbyteArray> j_data =
      ToJavaByteArray(env, data.data(), data_size);
  Java_FeedStreamSurface_onStreamUpdated(env, java_ref_, j_data);
}

void FeedStreamSurface::NavigationStarted(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj,
                                          const JavaParamRef<jstring>& url,
                                          jboolean in_new_tab) {}

void FeedStreamSurface::NavigationDone(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jstring>& url,
                                       jboolean in_new_tab) {}

void FeedStreamSurface::LoadMore(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj) {}

void FeedStreamSurface::ProcessThereAndBackAgain(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jbyteArray>& data) {}

int FeedStreamSurface::ExecuteEphemeralChange(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& data) {
  return 0;
}

void FeedStreamSurface::CommitEphemeralChange(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              int change_id) {}

void FeedStreamSurface::DiscardEphemeralChange(JNIEnv* env,
                                               const JavaParamRef<jobject>& obj,
                                               int change_id) {}

void FeedStreamSurface::SurfaceOpened(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {}

void FeedStreamSurface::SurfaceClosed(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {}

}  // namespace feed
