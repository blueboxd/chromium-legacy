// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {

// Native access to |FeedStreamSurface| in Java.
// Created once for each NTP/start surface.
class FeedStreamSurface {
 public:
  explicit FeedStreamSurface(const base::android::JavaRef<jobject>& j_this);
  ~FeedStreamSurface();

  void OnStreamUpdated(const feedui::StreamUpdate& stream_update);

  void NavigationStarted(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& caller,
                         const base::android::JavaParamRef<jstring>& url,
                         jboolean in_new_tab);

  void NavigationDone(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller,
                      const base::android::JavaParamRef<jstring>& url,
                      jboolean in_new_tab);

  void LoadMore(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& caller);

  void ProcessThereAndBackAgain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& data);

  int ExecuteEphemeralChange(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& caller,
                             const base::android::JavaParamRef<jobject>& data);

  void CommitEphemeralChange(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& caller,
                             int change_id);

  void DiscardEphemeralChange(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      int change_id);

  void SurfaceOpened(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& caller);

  void SurfaceClosed(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& caller);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  DISALLOW_COPY_AND_ASSIGN(FeedStreamSurface);
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
