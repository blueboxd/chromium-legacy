// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_HOST_FACTORY_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_HOST_FACTORY_H_

#include <memory>
#include <string>

namespace android_webview {

class WebMessageHost;
class WebMessageReplyProxy;

// Creates a WebMessageHost in response to a page interacting with the object
// registered by way of JsJavaConfiguratorHost::AddWebMessageHostFactory(). A
// WebMessageHost is created for every page that matches the parameters of
// AddWebMessageHostFactory().
class WebMessageHostFactory {
 public:
  virtual ~WebMessageHostFactory() {}

  // Creates a WebMessageHost for the specified page. |proxy| is valid for
  // the life of the host and may be used to send messages back to the page.
  virtual std::unique_ptr<WebMessageHost> CreateHost(
      const std::string& origin_string,
      bool is_main_frame,
      WebMessageReplyProxy* proxy) = 0;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_WEB_MESSAGE_HOST_FACTORY_H_
