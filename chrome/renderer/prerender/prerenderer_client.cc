// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerenderer_client.h"

#include "base/logging.h"
#include "chrome/renderer/prerender/prerender_extra_data.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

PrerendererClient::PrerendererClient(content::RenderView* render_view)
    : content::RenderViewObserver(render_view) {
  DCHECK(render_view);
  DVLOG(5) << "PrerendererClient::PrerendererClient()";
  render_view->GetWebView()->SetPrerendererClient(this);
}

PrerendererClient::~PrerendererClient() = default;

void PrerendererClient::WillAddPrerender(blink::WebLocalFrame* local_frame,
                                         blink::WebPrerender* prerender) {
  DVLOG(3) << "PrerendererClient::willAddPrerender url = "
           << prerender->Url().GetString().Utf8();
  prerender->SetExtraData(new PrerenderExtraData(
      content::RenderFrame::FromWebFrame(local_frame)->GetRoutingID()));
}

bool PrerendererClient::IsPrefetchOnly() {
  return PrerenderHelper::GetPrerenderMode(
             render_view()->GetMainRenderFrame()) == PREFETCH_ONLY;
}

void PrerendererClient::OnDestruct() {
  delete this;
}

}  // namespace prerender
