// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_

#include "services/network/public/mojom/web_sandbox_flags.mojom-shared.h"
#include "third_party/blink/public/common/feature_policy/document_policy_features.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"

namespace blink {

// This structure contains the attributes of a frame which determine what
// features are available during the lifetime of the framed document. Currently,
// this includes the sandbox flags, the feature policy container policy, and
// document policy required policy. Used in the frame tree to track sandbox,
// feature policy and document policy in the browser process, and transferred
// over IPC during frame replication when site isolation is enabled.
//
// Unlike the attributes in FrameOwnerProperties, these attributes are never
// updated after the framed document has been loaded, so two versions of this
// structure are kept in the frame tree for each frame -- the effective policy
// and the pending policy, which will take effect when the frame is next
// navigated.
struct BLINK_COMMON_EXPORT FramePolicy {
  FramePolicy();
  FramePolicy(network::mojom::WebSandboxFlags sandbox_flags,
              const ParsedFeaturePolicy& container_policy,
              const DocumentPolicyFeatureState& required_document_policy,
              bool allowed_to_download = true,
              bool disallow_document_access = false);
  FramePolicy(const FramePolicy& lhs);
  ~FramePolicy();

  network::mojom::WebSandboxFlags sandbox_flags;
  ParsedFeaturePolicy container_policy;
  // |required_document_policy| is the combination of the following:
  // - iframe 'policy' attribute
  // - 'Require-Document-Policy' http header
  // - |required_document_policy| of parent frame
  DocumentPolicyFeatureState required_document_policy;
  // With FeaturePolicyForSandbox, as a policy affecting the document,
  // "downloads" is included in |container_policy|.
  // However, in certain cases where the initiator of the navigation is not the
  // document itself (e.g., a parent document), the FrameOwner element should be
  // checked for "downloads" flag. If this boolean is false then navigations
  // leading to downloads should be blocked. Note: this flag is currently only
  // set if the frame is sandboxed for downloads.
  bool allowed_to_download;

  // Whether or not a frame allows direct script access across frame
  // boundaries.
  bool disallow_document_access = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_FRAME_FRAME_POLICY_H_
