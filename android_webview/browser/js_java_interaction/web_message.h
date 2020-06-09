// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_H_

#include <vector>

#include "base/strings/string16.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace android_webview {

// Represents a message to or from the page.
struct WebMessage {
  WebMessage();
  ~WebMessage();

  base::string16 message;
  std::vector<blink::MessagePortDescriptor> ports;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_H_
