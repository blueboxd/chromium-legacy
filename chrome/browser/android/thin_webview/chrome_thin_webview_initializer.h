// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_THIN_WEBVIEW_CHROME_THIN_WEBVIEW_INITIALIZER_H_
#define CHROME_BROWSER_ANDROID_THIN_WEBVIEW_CHROME_THIN_WEBVIEW_INITIALIZER_H_

#include "components/thin_webview/thin_webview_initializer.h"

namespace thin_webview {
namespace android {

// A helper class to help in attaching tab helpers.
class ChromeThinWebViewInitializer : public ThinWebViewInitializer {
 public:
  static void Initialize();

  ChromeThinWebViewInitializer() = default;
  ~ChromeThinWebViewInitializer() = default;

  void AttachTabHelpers(content::WebContents* web_contents) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeThinWebViewInitializer);
};

}  // namespace android
}  // namespace thin_webview

#endif  // CHROME_BROWSER_ANDROID_THIN_WEBVIEW_CHROME_THIN_WEBVIEW_INITIALIZER_H_
