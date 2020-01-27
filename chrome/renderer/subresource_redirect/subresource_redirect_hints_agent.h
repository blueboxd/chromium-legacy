// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_
#define CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom.h"
#include "url/gurl.h"

namespace subresource_redirect {

// Holds the public image URL hints to be queried by URL loader throttles.
class SubresourceRedirectHintsAgent {
 public:
  SubresourceRedirectHintsAgent();
  ~SubresourceRedirectHintsAgent();

  SubresourceRedirectHintsAgent(const SubresourceRedirectHintsAgent&) = delete;
  SubresourceRedirectHintsAgent& operator=(
      const SubresourceRedirectHintsAgent&) = delete;

  void SetCompressPublicImagesHints(
      blink::mojom::CompressPublicImagesHintsPtr images_hints);

  bool ShouldRedirectImage(const GURL& url) const;

 private:
  base::flat_set<std::string> public_image_urls_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_
