// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/loader/referrer_utils.h"

#include <atomic>

#include "base/command_line.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"

namespace {

// Using an atomic is necessary because this code is called from both the
// browser and the renderer (so that access is not on a single sequence when in
// single-process mode), and because it is called from multiple threads within
// the renderer.
bool ReadModifyWriteForceLegacyPolicyFlag(
    base::Optional<bool> maybe_new_value) {
  // Default to false in the browser process (it is not expected
  // that the browser will be provided this switch).
  // The value is propagated to other processes through the command line.
  DCHECK(base::CommandLine::InitializedForCurrentProcess());
  static std::atomic<bool> value(
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          blink::switches::kForceLegacyDefaultReferrerPolicy));
  if (!maybe_new_value.has_value())
    return value;
  return value.exchange(*maybe_new_value);
}

}  // namespace

namespace blink {

network::mojom::ReferrerPolicy ReferrerUtils::NetToMojoReferrerPolicy(
    net::ReferrerPolicy net_policy) {
  switch (net_policy) {
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case net::ReferrerPolicy::REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    case net::ReferrerPolicy::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case net::ReferrerPolicy::NEVER_CLEAR:
      return network::mojom::ReferrerPolicy::kAlways;
    case net::ReferrerPolicy::ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case net::ReferrerPolicy::CLEAR_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case net::ReferrerPolicy::
        ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case net::ReferrerPolicy::NO_REFERRER:
      return network::mojom::ReferrerPolicy::kNever;
  }
  NOTREACHED();
  return network::mojom::ReferrerPolicy::kDefault;
}

net::ReferrerPolicy ReferrerUtils::GetDefaultNetReferrerPolicy() {
  // The ReducedReferrerGranularity feature sets the default referrer
  // policy to strict-origin-when-cross-origin unless forbidden
  // by the "force legacy policy" global.
  // TODO(crbug.com/1016541) Once the pertinent enterprise policy has
  // been removed in M88, update this to remove the global.

  // Short-circuit to avoid acquiring the lock unless necessary.
  if (!base::FeatureList::IsEnabled(features::kReducedReferrerGranularity))
    return net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE;

  return ShouldForceLegacyDefaultReferrerPolicy()
             ? net::ReferrerPolicy::CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE
             : net::ReferrerPolicy::
                   REDUCE_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
}

void ReferrerUtils::SetForceLegacyDefaultReferrerPolicy(bool force) {
  ReadModifyWriteForceLegacyPolicyFlag(force);
}

bool ReferrerUtils::ShouldForceLegacyDefaultReferrerPolicy() {
  return ReadModifyWriteForceLegacyPolicyFlag(base::nullopt);
}

}  // namespace blink
