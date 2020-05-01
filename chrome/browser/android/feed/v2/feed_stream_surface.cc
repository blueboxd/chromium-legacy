// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/feed_stream_surface.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/FeedStreamSurface_jni.h"
#include "chrome/browser/android/feed/v2/feed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/feed/core/proto/v2/ui.pb.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/feed_stream_api.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace feed {

static jlong JNI_FeedStreamSurface_Init(JNIEnv* env,
                                        const JavaParamRef<jobject>& j_this) {
  return reinterpret_cast<intptr_t>(new FeedStreamSurface(j_this));
}

FeedStreamSurface::FeedStreamSurface(const JavaRef<jobject>& j_this)
    : feed_stream_api_(nullptr) {
  java_ref_.Reset(j_this);

  // TODO(iwells): check that this profile is okay to use. what about first run?
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile)
    return;

  feed_stream_api_ =
      FeedServiceFactory::GetForBrowserContext(profile)->GetStream();
}

FeedStreamSurface::~FeedStreamSurface() {
  if (feed_stream_api_)
    feed_stream_api_->DetachSurface(this);
}

void FeedStreamSurface::StreamUpdate(
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
                                      const JavaParamRef<jobject>& obj) {
  if (feed_stream_api_ && !attached_) {
    attached_ = true;
    feed_stream_api_->AttachSurface(this);
  }
}

void FeedStreamSurface::SurfaceClosed(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (feed_stream_api_ && attached_) {
    attached_ = false;
    feed_stream_api_->DetachSurface(this);
  }
}

void FeedStreamSurface::ReportOpenAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& slice_id) {
  feed_stream_api_->ReportOpenAction(
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStreamSurface::ReportOpenInNewTabAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& slice_id) {
  feed_stream_api_->ReportOpenInNewTabAction(
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStreamSurface::ReportOpenInNewIncognitoTabAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportOpenInNewIncognitoTabAction();
}

void FeedStreamSurface::ReportSliceViewed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& slice_id) {
  feed_stream_api_->ReportSliceViewed(
      base::android::ConvertJavaStringToUTF8(env, slice_id));
}

void FeedStreamSurface::ReportSendFeedbackAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportSendFeedbackAction();
}

void FeedStreamSurface::ReportLearnMoreAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportLearnMoreAction();
}

void FeedStreamSurface::ReportDownloadAction(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportDownloadAction();
}

void FeedStreamSurface::ReportNavigationStarted(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& url,
    jboolean in_new_tab) {
  feed_stream_api_->ReportNavigationStarted();
}

void FeedStreamSurface::ReportNavigationDone(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             const JavaParamRef<jstring>& url,
                                             jboolean in_new_tab) {
  feed_stream_api_->ReportNavigationDone();
}

void FeedStreamSurface::ReportRemoveAction(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportRemoveAction();
}

void FeedStreamSurface::ReportNotInterestedInAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportNotInterestedInAction();
}

void FeedStreamSurface::ReportManageInterestsAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportManageInterestsAction();
}

void FeedStreamSurface::ReportContextMenuOpened(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  feed_stream_api_->ReportContextMenuOpened();
}

void FeedStreamSurface::ReportStreamScrolled(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj,
                                             int distance_dp) {
  feed_stream_api_->ReportStreamScrolled(distance_dp);
}

}  // namespace feed
