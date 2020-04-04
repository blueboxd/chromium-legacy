// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/feed/core/v2/public/feed_stream_api.h"

namespace feedui {
class StreamUpdate;
}

namespace feed {

// Native access to |FeedStreamSurface| in Java.
// Created once for each NTP/start surface.
class FeedStreamSurface : public FeedStreamApi::SurfaceInterface {
 public:
  explicit FeedStreamSurface(const base::android::JavaRef<jobject>& j_this);
  FeedStreamSurface(const FeedStreamSurface&) = delete;
  FeedStreamSurface& operator=(const FeedStreamSurface&) = delete;

  ~FeedStreamSurface() override;

  // SurfaceInterface implementation.
  void StreamUpdate(const feedui::StreamUpdate& update) override;

  void OnStreamUpdated(const feedui::StreamUpdate& stream_update);

  void LoadMore(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void ProcessThereAndBackAgain(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& data);

  int ExecuteEphemeralChange(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             const base::android::JavaParamRef<jobject>& data);

  void CommitEphemeralChange(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj,
                             int change_id);

  void DiscardEphemeralChange(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              int change_id);

  void SurfaceOpened(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  void SurfaceClosed(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  // Event reporting functions. These have no side-effect beyond recording
  // metrics.

  void ReportNavigationStarted(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               const base::android::JavaParamRef<jstring>& url,
                               jboolean in_new_tab);

  void ReportNavigationDone(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj,
                            const base::android::JavaParamRef<jstring>& url,
                            jboolean in_new_tab);

  // A piece of content was removed or dismissed explicitly by the user.
  void ReportContentRemoved(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  // The 'Not Interested In' menu item was selected.
  void ReportNotInterestedIn(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  // The 'Manage Interests' menu item was selected.
  void ReportManageInterests(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);

  // The user opened the context menu (three dot, or long press).
  void ReportContextMenuOpened(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
  FeedStreamApi* feed_stream_api_;
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_FEED_STREAM_SURFACE_H_
