// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PRERENDER_PRERENDER_EXTRA_DATA_H_
#define CHROME_RENDERER_PRERENDER_PRERENDER_EXTRA_DATA_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/common/prerender.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_prerender.h"
#include "ui/gfx/geometry/size.h"

namespace prerender {

class PrerenderExtraData : public blink::WebPrerender::ExtraData {
 public:
  explicit PrerenderExtraData(int render_view_id);
  ~PrerenderExtraData() override;

  void set_prerender_handle(
      mojo::Remote<chrome::mojom::PrerenderHandle> prerender_handle) {
    prerender_handle_ = std::move(prerender_handle);
  }
  chrome::mojom::PrerenderHandle* prerender_handle() {
    return prerender_handle_.get();
  }

  int render_view_id() const { return render_view_id_; }

  static PrerenderExtraData* FromPrerender(
      const blink::WebPrerender& prerender);

 private:
  mojo::Remote<chrome::mojom::PrerenderHandle> prerender_handle_;
  const int render_view_id_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderExtraData);
};

}  // namespace prerender

#endif  // CHROME_RENDERER_PRERENDER_PRERENDER_EXTRA_DATA_H_

