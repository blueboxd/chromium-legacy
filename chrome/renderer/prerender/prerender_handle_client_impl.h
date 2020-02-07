// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_HANDLE_CLIENT_IMPL_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_HANDLE_CLIENT_IMPL_H_

#include "chrome/common/prerender.mojom.h"
#include "third_party/blink/public/platform/web_prerender.h"

namespace prerender {

class PrerenderHandleClientImpl : public chrome::mojom::PrerenderHandleClient {
 public:
  explicit PrerenderHandleClientImpl(const blink::WebPrerender& web_prerender);
  ~PrerenderHandleClientImpl() override;

  // chrome::mojom::PrerenderHandleClient implementation
  void OnPrerenderStart() override;
  void OnPrerenderStopLoading() override;
  void OnPrerenderDomContentLoaded() override;
  void OnPrerenderStop() override;

 private:
  blink::WebPrerender web_prerender_;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_HANDLE_CLIENT_IMPL_H_
