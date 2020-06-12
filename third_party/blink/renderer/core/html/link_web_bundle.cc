// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/link_web_bundle.h"

namespace blink {

LinkWebBundle::LinkWebBundle(HTMLLinkElement* owner) : LinkResource(owner) {}

void LinkWebBundle::Process() {
  // TODO(crbug.com/1082020): Implement this.
}

LinkResource::LinkResourceType LinkWebBundle::GetType() const {
  return kOther;
}

bool LinkWebBundle::HasLoaded() const {
  return false;
}

void LinkWebBundle::OwnerRemoved() {}

// static
KURL LinkWebBundle::ParseResourceUrl(const AtomicString& str) {
  // The implementation is almost copy and paste from ParseExchangeURL() defined
  // in services/data_decoder/web_bundle_parser.cc, replacing GURL with KURL.

  // TODO(hayato): Consider to support a relative URL.
  KURL url(str);
  if (!url.IsValid())
    return KURL();

  // Exchange URL must not have a fragment or credentials.
  if (url.HasFragmentIdentifier() || !url.User().IsEmpty() ||
      !url.Pass().IsEmpty())
    return KURL();

  // For now, we allow only http: and https: schemes in Web Bundle URLs.
  // TODO(crbug.com/966753): Revisit this once
  // https://github.com/WICG/webpackage/issues/468 is resolved.
  if (!url.ProtocolIsInHTTPFamily())
    return KURL();

  return url;
}

}  // namespace blink
