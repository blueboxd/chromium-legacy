// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapps/webapk_ukm_recorder.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/WebApkUkmRecorder_jni.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

// static
void WebApkUkmRecorder::RecordSessionDuration(const GURL& manifest_url,
                                              int64_t distributor,
                                              int64_t version_code,
                                              int64_t duration) {
  if (!manifest_url.is_valid())
    return;

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, manifest_url);
  ukm::builders::WebAPK_SessionEnd(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetSessionDuration(ukm::GetExponentialBucketMinForUserTiming(duration))
      .Record(ukm_recorder);
}

// Called by the Java counterpart to record the Session Duration UKM metric.
void JNI_WebApkUkmRecorder_RecordSessionDuration(
    JNIEnv* env,
    const JavaParamRef<jstring>& manifest_url,
    jint distributor,
    jint version_code,
    jlong duration) {
  if (!manifest_url)
    return;

  WebApkUkmRecorder::RecordSessionDuration(
      GURL(base::android::ConvertJavaStringToUTF8(env, manifest_url)),
      distributor, version_code, duration);
}
