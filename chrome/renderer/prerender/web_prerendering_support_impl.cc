// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/web_prerendering_support_impl.h"

#include "chrome/renderer/prerender/prerender_extra_data.h"
#include "chrome/renderer/prerender/prerender_handle_client_impl.h"
#include "content/public/common/referrer.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

WebPrerenderingSupportImpl::WebPrerenderingSupportImpl() {
  blink::WebPrerenderingSupport::Initialize(this);
}

WebPrerenderingSupportImpl::~WebPrerenderingSupportImpl() {
  blink::WebPrerenderingSupport::Shutdown();
}

void WebPrerenderingSupportImpl::Add(const blink::WebPrerender& prerender) {
  PrerenderExtraData* extra_data = PrerenderExtraData::FromPrerender(prerender);
  content::RenderFrame* render_frame =
      content::RenderFrame::FromRoutingID(extra_data->render_frame_id());
  if (!render_frame)
    return;

  chrome::mojom::PrerenderAttributesPtr attributes =
      chrome::mojom::PrerenderAttributes::New();
  attributes->url = prerender.Url();
  attributes->rel_types = prerender.RelTypes();
  attributes->referrer = content::Referrer::SanitizeForRequest(
      attributes->url, *blink::mojom::Referrer::New(
                           blink::WebStringToGURL(prerender.GetReferrer()),
                           prerender.GetReferrerPolicy()));
  attributes->initiator_origin = prerender.SecurityOrigin();
  attributes->view_size = render_frame->GetWebFrame()->View()->GetSize();

  mojo::Remote<chrome::mojom::PrerenderProcessor> prerender_processor;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      prerender_processor.BindNewPipeAndPassReceiver());

  // We let the remote end own the lifetime of the client.
  mojo::PendingRemote<chrome::mojom::PrerenderHandleClient>
      prerender_handle_client;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrerenderHandleClientImpl>(prerender),
      prerender_handle_client.InitWithNewPipeAndPassReceiver());

  mojo::Remote<chrome::mojom::PrerenderHandle> prerender_handle;
  prerender_processor->AddPrerender(
      std::move(attributes), std::move(prerender_handle_client),
      prerender_handle.BindNewPipeAndPassReceiver());

  // We store the handle on |extra_data| enabling us to find it again in
  // support of Abandon and Cancel methods. See below.
  extra_data->set_prerender_handle(std::move(prerender_handle));
}

void WebPrerenderingSupportImpl::Cancel(const blink::WebPrerender& prerender) {
  PrerenderExtraData* extra_data = PrerenderExtraData::FromPrerender(prerender);
  extra_data->prerender_handle()->Cancel();
}

void WebPrerenderingSupportImpl::Abandon(const blink::WebPrerender& prerender) {
  PrerenderExtraData* extra_data = PrerenderExtraData::FromPrerender(prerender);
  extra_data->prerender_handle()->Abandon();
}

}  // namespace prerender
