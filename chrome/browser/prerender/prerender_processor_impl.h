// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_PROCESSOR_IMPL_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_PROCESSOR_IMPL_H_

#include "chrome/common/prerender.mojom.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace prerender {

class PrerenderProcessorImpl : public chrome::mojom::PrerenderProcessor {
 public:
  PrerenderProcessorImpl(int render_process_id, int render_frame_id);
  ~PrerenderProcessorImpl() override;

  static void Create(
      content::RenderFrameHost* frame_host,
      mojo::PendingReceiver<chrome::mojom::PrerenderProcessor> receiver);

  // chrome::mojom::PrerenderProcessor implementation
  void AddPrerender(
      chrome::mojom::PrerenderAttributesPtr attributes,
      mojo::PendingRemote<chrome::mojom::PrerenderHandleClient> client,
      mojo::PendingReceiver<chrome::mojom::PrerenderHandle> handle) override;

 private:
  int render_process_id_;
  int render_frame_id_;
};

}  // namespace prerender

#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_PROCESSOR_IMPL_H_
