// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_api_handler.h"

#include "android_webview/browser/aw_contents.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {

JsApiHandler::JsApiHandler(content::RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

JsApiHandler::~JsApiHandler() {}

void JsApiHandler::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::JsApiHandler> pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

void JsApiHandler::PostMessage(
    const std::string& message,
    std::vector<mojo::ScopedMessagePipeHandle> ports) {
  DCHECK(render_frame_host_);

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host_);
  AwContents* aw_contents = AwContents::FromWebContents(web_contents);

  if (!aw_contents)
    return;

  // |source_origin| has no race with this PostMessage call, because of
  // associated mojo channel, the committed origin message and PostMessage are
  // in sequence.
  url::Origin source_origin = render_frame_host_->GetLastCommittedOrigin();

  if (!aw_contents->IsOriginAllowedForOnPostMessage(source_origin))
    return;

  std::vector<int> int_ports(ports.size(), MOJO_HANDLE_INVALID /* 0 */);
  for (size_t i = 0; i < ports.size(); ++i) {
    int_ports[i] = ports[i].release().value();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  aw_contents->OnPostMessage(
      env, base::android::ConvertUTF8ToJavaString(env, message),
      base::android::ConvertUTF8ToJavaString(env, source_origin.Serialize()),
      web_contents->GetMainFrame() == render_frame_host_,
      base::android::ToJavaIntArray(env, int_ports.data(), int_ports.size()));
}

}  // namespace android_webview
