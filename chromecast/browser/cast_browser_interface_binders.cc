// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_browser_interface_binders.h"

#include "base/bind.h"
#include "chromecast/browser/application_media_capabilities.h"
#include "chromecast/browser/cast_web_contents.h"
#include "chromecast/common/mojom/application_media_capabilities.mojom.h"
#include "components/network_hints/browser/simple_network_hints_handler_impl.h"
#include "components/network_hints/common/network_hints.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace chromecast {
namespace shell {

namespace {

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  network_hints::SimpleNetworkHintsHandlerImpl::Create(frame_host,
                                                       std::move(receiver));
}

void BindApplicationMediaCapabilities(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mojom::ApplicationMediaCapabilities> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;
  auto* cast_web_contents = CastWebContents::FromWebContents(web_contents);
  if (!cast_web_contents || !cast_web_contents->can_bind_interfaces())
    return;
  auto interface_pipe = receiver.PassPipe();
  cast_web_contents->binder_registry()->TryBindInterface(
      mojom::ApplicationMediaCapabilities::Name_, &interface_pipe);
}

}  // namespace

void PopulateCastFrameBinders(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map) {
  binder_map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));
  binder_map->Add<mojom::ApplicationMediaCapabilities>(
      base::BindRepeating(&BindApplicationMediaCapabilities));
}

}  // namespace shell
}  // namespace chromecast
