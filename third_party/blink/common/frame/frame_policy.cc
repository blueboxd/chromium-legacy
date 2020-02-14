// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/frame/frame_policy.h"

namespace blink {

FramePolicy::FramePolicy()
    : sandbox_flags(mojom::WebSandboxFlags::kNone),
      container_policy({}),
      required_document_policy({}),
      allowed_to_download(true),
      disallow_document_access(false) {}

FramePolicy::FramePolicy(
    mojom::WebSandboxFlags sandbox_flags,
    const ParsedFeaturePolicy& container_policy,
    const DocumentPolicy::FeatureState& required_document_policy,
    bool allowed_to_download,
    bool disallow_document_access)
    : sandbox_flags(sandbox_flags),
      container_policy(container_policy),
      required_document_policy(required_document_policy),
      allowed_to_download(allowed_to_download),
      disallow_document_access(disallow_document_access) {}

FramePolicy::FramePolicy(const FramePolicy& lhs) = default;

FramePolicy::~FramePolicy() = default;

}  // namespace blink
