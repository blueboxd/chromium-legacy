// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_BINDING_H_
#define ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_BINDING_H_

#include <string>

#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/auto_reset.h"
#include "gin/arguments.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace android_webview {

// A gin::Wrappable class used for providing JavaScript API. We will inject the
// object of this class to JavaScript world in JsJavaConfigurator.
// JsJavaConfigurator will own at most one instance of this class. When the
// RenderFrame gone or another DidClearWindowObject comes, the instance will be
// destroyed.
class JsBinding : public gin::Wrappable<JsBinding> {
 public:
  static gin::WrapperInfo kWrapperInfo;
  static std::unique_ptr<JsBinding> Install(content::RenderFrame* render_frame,
                                            const std::string& js_object_name);

  ~JsBinding() final;

 private:
  explicit JsBinding(content::RenderFrame* render_frame);

  // gin::Wrappable implementation
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // For jsObject.postMessage(message[, ports]) JavaScript API.
  void PostMessage(gin::Arguments* args);

  content::RenderFrame* render_frame_;

  mojo::AssociatedRemote<mojom::JsApiHandler> js_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(JsBinding);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_RENDERER_JS_JAVA_INTERACTION_JS_BINDING_H_
