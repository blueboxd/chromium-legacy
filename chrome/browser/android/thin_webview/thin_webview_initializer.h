// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_

#include "base/macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace thin_webview {
namespace android {

// A helper class to help in attaching tab helpers.
class ThinWebViewInitializer {
 public:
  static void SetInstance(ThinWebViewInitializer* instance);
  static ThinWebViewInitializer* GetInstance();

  ThinWebViewInitializer() = default;
  ~ThinWebViewInitializer() = default;

  virtual void AttachTabHelpers(content::WebContents* web_contents) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ThinWebViewInitializer);
};

}  // namespace android
}  // namespace thin_webview

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_THIN_WEBVIEW_INITIALIZER_H_
