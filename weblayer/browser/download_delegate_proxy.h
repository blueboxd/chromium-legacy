// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_BROWSER_DOWNLOAD_DELEGATE_PROXY_H_
#define WEBLAYER_BROWSER_DOWNLOAD_DELEGATE_PROXY_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

class BrowserController;

// Forwards DownloadDelegate calls to the java-side DownloadDelegateProxy.
class DownloadDelegateProxy : public DownloadDelegate {
 public:
  DownloadDelegateProxy(JNIEnv* env,
                        jobject obj,
                        BrowserController* browser_controller);
  ~DownloadDelegateProxy() override;

  // DownloadDelegate:
  void DownloadRequested(const GURL& url,
                         const std::string& user_agent,
                         const std::string& content_disposition,
                         const std::string& mime_type,
                         int64_t content_length) override;

 private:
  BrowserController* browser_controller_;
  base::android::ScopedJavaGlobalRef<jobject> java_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DownloadDelegateProxy);
};

}  // namespace weblayer

#endif  // WEBLAYER_BROWSER_DOWNLOAD_DELEGATE_PROXY_H_
