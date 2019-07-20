// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/js_java_interaction/js_api_handler_factory.h"

#include "android_webview/browser/js_java_interaction/js_api_handler.h"
#include "content/public/browser/web_contents.h"

namespace android_webview {
namespace {
const void* const kUserDataKey = &kUserDataKey;
}

JsApiHandlerFactory::JsApiHandlerFactory(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  for (content::RenderFrameHost* render_frame_host :
       web_contents->GetAllFrames()) {
    RenderFrameCreated(render_frame_host);
  }
}

JsApiHandlerFactory::~JsApiHandlerFactory() = default;

// static
JsApiHandlerFactory* JsApiHandlerFactory::FromWebContents(
    content::WebContents* web_contents) {
  JsApiHandlerFactory* factory = static_cast<JsApiHandlerFactory*>(
      web_contents->GetUserData(kUserDataKey));

  if (!factory) {
    factory = new JsApiHandlerFactory(web_contents);
    web_contents->SetUserData(kUserDataKey, base::WrapUnique(factory));
  }
  return factory;
}

// static
void JsApiHandlerFactory::BindJsApiHandler(
    mojo::PendingAssociatedReceiver<mojom::JsApiHandler> pending_receiver,
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  if (!web_contents)
    return;

  JsApiHandlerFactory* factory =
      JsApiHandlerFactory::FromWebContents(web_contents);
  if (!factory)
    return;

  JsApiHandler* handler = factory->JsApiHandlerForFrame(render_frame_host);
  if (handler)
    handler->BindPendingReceiver(std::move(pending_receiver));
}

void JsApiHandlerFactory::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  if (JsApiHandlerForFrame(render_frame_host))
    return;

  frame_map_[render_frame_host] =
      std::make_unique<JsApiHandler>(render_frame_host);
}

void JsApiHandlerFactory::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  frame_map_.erase(render_frame_host);
}

JsApiHandler* JsApiHandlerFactory::JsApiHandlerForFrame(
    content::RenderFrameHost* render_frame_host) {
  auto itr = frame_map_.find(render_frame_host);
  return itr == frame_map_.end() ? nullptr : itr->second.get();
}

}  // namespace android_webview
