// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/renderer/js_java_interaction/js_binding.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "content/public/renderer/render_frame.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_message_port_converter.h"
#include "v8/include/v8.h"

namespace {
constexpr char kPostMessage[] = "postMessage";
}  // anonymous namespace

namespace android_webview {

gin::WrapperInfo JsBinding::kWrapperInfo = {gin::kEmbedderNativeGin};

// static
std::unique_ptr<JsBinding> JsBinding::Install(
    content::RenderFrame* render_frame,
    const std::string& js_object_name) {
  CHECK(!js_object_name.empty())
      << "JavaScript wrapper name shouldn't be empty";

  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context =
      render_frame->GetWebFrame()->MainWorldScriptContext();
  if (context.IsEmpty())
    return nullptr;

  v8::Context::Scope context_scope(context);
  std::unique_ptr<JsBinding> js_binding(new JsBinding(render_frame));
  gin::Handle<JsBinding> bindings =
      gin::CreateHandle(isolate, js_binding.get());
  if (bindings.IsEmpty())
    return nullptr;

  v8::Local<v8::Object> global = context->Global();
  global
      ->CreateDataProperty(context,
                           gin::StringToSymbol(isolate, js_object_name),
                           bindings.ToV8())
      .Check();

  return js_binding;
}

JsBinding::JsBinding(content::RenderFrame* render_frame)
    : render_frame_(render_frame) {}

JsBinding::~JsBinding() = default;

gin::ObjectTemplateBuilder JsBinding::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<JsBinding>::GetObjectTemplateBuilder(isolate).SetMethod(
      kPostMessage, &JsBinding::PostMessage);
}

void JsBinding::PostMessage(gin::Arguments* args) {
  std::string message;
  if (!args->GetNext(&message)) {
    args->ThrowError();
    return;
  }

  std::vector<blink::MessagePortChannel> ports;
  std::vector<v8::Local<v8::Object>> objs;
  // If we get more than two arguments and the second argument is not an array
  // of ports, we can't process.
  if (args->Length() >= 2 && !args->GetNext(&objs)) {
    args->ThrowError();
    return;
  }

  for (auto& obj : objs) {
    base::Optional<blink::MessagePortChannel> port =
        blink::WebMessagePortConverter::DisentangleAndExtractMessagePortChannel(
            args->isolate(), obj);
    // If the port is null we should throw an exception.
    if (!port.has_value()) {
      args->ThrowError();
      return;
    }
    ports.emplace_back(port.value());
  }

  if (!js_api_handler_) {
    render_frame_->GetRemoteAssociatedInterfaces()->GetInterface(
        &js_api_handler_);
  }

  js_api_handler_->PostMessage(
      message, blink::MessagePortChannel::ReleaseHandles(ports));
}

}  // namespace android_webview
