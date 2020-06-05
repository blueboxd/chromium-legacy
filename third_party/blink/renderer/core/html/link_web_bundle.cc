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

}  // namespace blink
