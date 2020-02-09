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
  enum class RedirectResult {
    // The image was found in the image hints and is eligible to be redirected
    // to compressed version.
    kRedirectable,

    // Possible reasons for ineligibility.
    // because the image hint list was not retrieved at the time of image fetch.
    kIneligibleImageHintsUnavailable,

    // because the the image URL was not found in the image hints.
    kIneligibleMissingInImageHints,

    // because of other reasons such as subframe images, Blink did not allow the
    // redirect due to non <img> element, security limitations, javascript
    // initiated image, etc.
    kIneligibleOtherImage
  };

  SubresourceRedirectHintsAgent();
  ~SubresourceRedirectHintsAgent();

  SubresourceRedirectHintsAgent(const SubresourceRedirectHintsAgent&) = delete;
  SubresourceRedirectHintsAgent& operator=(
      const SubresourceRedirectHintsAgent&) = delete;

  void SetCompressPublicImagesHints(
      blink::mojom::CompressPublicImagesHintsPtr images_hints);

  RedirectResult ShouldRedirectImage(const GURL& url) const;

  void RecordMetrics(int render_frame_id,
                     int64_t content_length,
                     RedirectResult redirect_result) const;

 private:
  bool public_image_urls_received_ = false;
  base::flat_set<std::string> public_image_urls_;
};

}  // namespace subresource_redirect

#endif  // CHROME_RENDERER_SUBRESOURCE_REDIRECT_SUBRESOURCE_REDIRECT_HINTS_AGENT_H_
