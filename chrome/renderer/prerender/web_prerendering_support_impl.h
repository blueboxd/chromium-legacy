// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_WEB_PRERENDERING_SUPPORT_IMPL_H_
#define CHROME_RENDERER_PRERENDER_WEB_PRERENDERING_SUPPORT_IMPL_H_

#include "chrome/common/prerender.mojom.h"
#include "third_party/blink/public/platform/web_prerendering_support.h"

namespace prerender {

// There is one WebPrerenderingSupportImpl per render process.
class WebPrerenderingSupportImpl : public blink::WebPrerenderingSupport {
 public:
  WebPrerenderingSupportImpl();
  ~WebPrerenderingSupportImpl() override;

 private:
  // From WebPrerenderingSupport:
  void Add(const blink::WebPrerender& prerender) override;
  void Cancel(const blink::WebPrerender& prerender) override;
  void Abandon(const blink::WebPrerender& prerender) override;
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_WEB_PRERENDERING_SUPPORT_IMPL_H_
