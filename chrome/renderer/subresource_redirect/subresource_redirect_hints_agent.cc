// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/subresource_redirect/subresource_redirect_hints_agent.h"

namespace subresource_redirect {

SubresourceRedirectHintsAgent::SubresourceRedirectHintsAgent() = default;
SubresourceRedirectHintsAgent::~SubresourceRedirectHintsAgent() = default;

void SubresourceRedirectHintsAgent::SetCompressPublicImagesHints(
    blink::mojom::CompressPublicImagesHintsPtr images_hints) {
  public_image_urls_ = images_hints->image_urls;
}

bool SubresourceRedirectHintsAgent::ShouldRedirectImage(const GURL& url) const {
  GURL::Replacements rep;
  rep.ClearRef();
  // TODO(rajendrant): Skip redirection if the URL contains username or password
  return public_image_urls_.find(url.ReplaceComponents(rep).spec()) !=
         public_image_urls_.end();
}

}  // namespace subresource_redirect
