// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_handle_client_impl.h"

namespace prerender {

PrerenderHandleClientImpl::PrerenderHandleClientImpl(
    const blink::WebPrerender& web_prerender)
    : web_prerender_(web_prerender) {}

PrerenderHandleClientImpl::~PrerenderHandleClientImpl() = default;

void PrerenderHandleClientImpl::OnPrerenderStart() {
  web_prerender_.DidStartPrerender();
}

void PrerenderHandleClientImpl::OnPrerenderStopLoading() {
  web_prerender_.DidSendLoadForPrerender();
}

void PrerenderHandleClientImpl::OnPrerenderDomContentLoaded() {
  web_prerender_.DidSendDOMContentLoadedForPrerender();
}

void PrerenderHandleClientImpl::OnPrerenderStop() {
  web_prerender_.DidStopPrerender();
}

}  // namespace prerender
