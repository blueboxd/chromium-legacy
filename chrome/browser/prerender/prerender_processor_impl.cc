// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_processor_impl.h"

#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace prerender {

PrerenderProcessorImpl::PrerenderProcessorImpl(int render_process_id,
                                               int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}

PrerenderProcessorImpl::~PrerenderProcessorImpl() = default;

// static
void PrerenderProcessorImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::PrerenderProcessor> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrerenderProcessorImpl>(
          frame_host->GetProcess()->GetID(), frame_host->GetRoutingID()),
      std::move(receiver));
}

void PrerenderProcessorImpl::AddPrerender(
    blink::mojom::PrerenderAttributesPtr attributes,
    mojo::PendingRemote<blink::mojom::PrerenderHandleClient> handle_client,
    mojo::PendingReceiver<blink::mojom::PrerenderHandle> handle) {
  if (!attributes->initiator_origin.opaque() &&
      !content::ChildProcessSecurityPolicy::GetInstance()
           ->CanAccessDataForOrigin(render_process_id_,
                                    attributes->initiator_origin.GetURL())) {
    mojo::ReportBadMessage("PMF_INVALID_INITIATOR_ORIGIN");
    return;
  }

  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  if (!render_frame_host)
    return;

  PrerenderLinkManager* link_manager =
      PrerenderLinkManagerFactory::GetForBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());
  if (!link_manager)
    return;

  link_manager->OnAddPrerender(
      render_process_id_,
      render_frame_host->GetRenderViewHost()->GetRoutingID(),
      std::move(attributes), std::move(handle_client), std::move(handle));
}

}  // namespace prerender
