// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_REPLY_PROXY_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_REPLY_PROXY_H_

#include "base/strings/string16.h"

namespace android_webview {

struct WebMessage;

// Used to send messages to the page.
class WebMessageReplyProxy {
 public:
  virtual void PostMessage(std::unique_ptr<WebMessage> message) = 0;

 protected:
  virtual ~WebMessageReplyProxy() {}
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_REPLY_PROXY_H_
