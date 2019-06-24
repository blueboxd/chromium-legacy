// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/UkmRecorder_jni.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace browserservices {

// Called by Java org.chromium.chrome.browser.browserservices.UkmRecorder.
static void JNI_UkmRecorder_RecordOpen(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);
  ukm::builders::TrustedWebActivity_Open(source_id).Record(
      ukm::UkmRecorder::Get());
}

}  // namespace browserservices
