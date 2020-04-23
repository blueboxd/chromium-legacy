/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/frame/sandbox_flags.h"

#include "services/network/public/cpp/web_sandbox_flags.h"
#include "services/network/public/mojom/web_sandbox_flags.mojom-blink.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"

namespace blink {

const SandboxFlagFeaturePolicyPairs& SandboxFlagsWithFeaturePolicies() {
  DEFINE_STATIC_LOCAL(
      SandboxFlagFeaturePolicyPairs, array,
      ({
          {network::mojom::blink::WebSandboxFlags::kTopNavigation,
           mojom::blink::FeaturePolicyFeature::kTopNavigation},
          {network::mojom::blink::WebSandboxFlags::kForms,
           mojom::blink::FeaturePolicyFeature::kFormSubmission},
          {network::mojom::blink::WebSandboxFlags::kScripts,
           mojom::blink::FeaturePolicyFeature::kScript},
          {network::mojom::blink::WebSandboxFlags::kPopups,
           mojom::blink::FeaturePolicyFeature::kPopups},
          {network::mojom::blink::WebSandboxFlags::kPointerLock,
           mojom::blink::FeaturePolicyFeature::kPointerLock},
          {network::mojom::blink::WebSandboxFlags::kModals,
           mojom::blink::FeaturePolicyFeature::kModals},
          {network::mojom::blink::WebSandboxFlags::kOrientationLock,
           mojom::blink::FeaturePolicyFeature::kOrientationLock},
          {network::mojom::blink::WebSandboxFlags::kPresentationController,
           mojom::blink::FeaturePolicyFeature::kPresentation},
          {network::mojom::blink::WebSandboxFlags::kDownloads,
           mojom::blink::FeaturePolicyFeature::kDownloads},
      }));
  return array;
}

// This returns a super mask which indicates the set of all flags that have
// corresponding feature policies. With FeaturePolicyForSandbox, these flags
// are always removed from the set of sandbox flags set for a sandboxed
// <iframe> (those sandbox flags are now contained in the |ContainerPolicy|).
network::mojom::blink::WebSandboxFlags
SandboxFlagsImplementedByFeaturePolicy() {
  DEFINE_STATIC_LOCAL(network::mojom::blink::WebSandboxFlags, mask,
                      (network::mojom::blink::WebSandboxFlags::kNone));
  if (mask == network::mojom::blink::WebSandboxFlags::kNone) {
    for (const auto& pair : SandboxFlagsWithFeaturePolicies())
      mask |= pair.first;
  }
  return mask;
}

// Removes a certain set of flags from |sandbox_flags| for which we have feature
// policies implemented.
network::mojom::blink::WebSandboxFlags
GetSandboxFlagsNotImplementedAsFeaturePolicy(
    network::mojom::blink::WebSandboxFlags sandbox_flags) {
  // Punch all the sandbox flags which are converted to feature policy.
  return sandbox_flags & ~SandboxFlagsImplementedByFeaturePolicy();
}

void ApplySandboxFlagsToParsedFeaturePolicy(
    network::mojom::blink::WebSandboxFlags sandbox_flags,
    ParsedFeaturePolicy& parsed_feature_policy) {
  for (const auto& pair : SandboxFlagsWithFeaturePolicies()) {
    if ((sandbox_flags & pair.first) !=
        network::mojom::blink::WebSandboxFlags::kNone)
      DisallowFeatureIfNotPresent(pair.second, parsed_feature_policy);
  }
}

}  // namespace blink
