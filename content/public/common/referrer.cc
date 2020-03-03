// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/referrer.h"

#include <atomic>
#include <string>

#include "base/numerics/safe_conversions.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/enum_utils.h"
#include "net/base/features.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/referrer.mojom.h"

namespace content {

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
          switches::kForceLegacyDefaultReferrerPolicy));
  if (!maybe_new_value.has_value())
    return value;
  return value.exchange(*maybe_new_value);
}

}  // namespace

Referrer::Referrer(const blink::mojom::Referrer& referrer)
    : url(referrer.url), policy(referrer.policy) {}

// static
Referrer Referrer::SanitizeForRequest(const GURL& request,
                                      const Referrer& referrer) {
  blink::mojom::ReferrerPtr sanitized_referrer = SanitizeForRequest(
      request, blink::mojom::Referrer(referrer.url, referrer.policy));
  return Referrer(sanitized_referrer->url, sanitized_referrer->policy);
}

// static
blink::mojom::ReferrerPtr Referrer::SanitizeForRequest(
    const GURL& request,
    const blink::mojom::Referrer& referrer) {
  blink::mojom::ReferrerPtr sanitized_referrer = blink::mojom::Referrer::New(
      referrer.url.GetAsReferrer(), referrer.policy);
  if (sanitized_referrer->policy == network::mojom::ReferrerPolicy::kDefault) {
    sanitized_referrer->policy =
        Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
            Referrer::GetDefaultReferrerPolicy());
  }

  if (sanitized_referrer->policy < network::mojom::ReferrerPolicy::kMinValue ||
      sanitized_referrer->policy > network::mojom::ReferrerPolicy::kMaxValue) {
    NOTREACHED();
    sanitized_referrer->policy = network::mojom::ReferrerPolicy::kNever;
  }

  bool is_web_scheme = request.SchemeIsHTTPOrHTTPS() || request.IsAboutBlank();
  if (!is_web_scheme || !sanitized_referrer->url.SchemeIsValidForReferrer()) {
    sanitized_referrer->url = GURL();
    return sanitized_referrer;
  }

  bool is_downgrade = sanitized_referrer->url.SchemeIsCryptographic() &&
                      !request.SchemeIsCryptographic();

  switch (sanitized_referrer->policy) {
    case network::mojom::ReferrerPolicy::kDefault:
      NOTREACHED();
      break;
    case network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade:
      if (is_downgrade)
        sanitized_referrer->url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kAlways:
      break;
    case network::mojom::ReferrerPolicy::kNever:
      sanitized_referrer->url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kOrigin:
      sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      break;
    case network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin:
      if (request.GetOrigin() != sanitized_referrer->url.GetOrigin())
        sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      break;
    case network::mojom::ReferrerPolicy::kStrictOrigin:
      if (is_downgrade) {
        sanitized_referrer->url = GURL();
      } else {
        sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      }
      break;
    case network::mojom::ReferrerPolicy::kSameOrigin:
      if (request.GetOrigin() != sanitized_referrer->url.GetOrigin())
        sanitized_referrer->url = GURL();
      break;
    case network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin:
      if (is_downgrade) {
        sanitized_referrer->url = GURL();
      } else if (request.GetOrigin() != sanitized_referrer->url.GetOrigin()) {
        sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
      }
      break;
  }

  // We limit the `referer` header to 4k: see step  of
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  // and https://github.com/whatwg/fetch/issues/903.
  if (sanitized_referrer->url.spec().length() > 4096 ||
      (base::FeatureList::IsEnabled(
           network::features::kCapReferrerToOriginOnCrossOrigin) &&
       !url::Origin::Create(sanitized_referrer->url)
            .IsSameOriginWith(url::Origin::Create(request)))) {
    sanitized_referrer->url = sanitized_referrer->url.GetOrigin();
  }

  return sanitized_referrer;
}

// static
url::Origin Referrer::SanitizeOriginForRequest(
    const GURL& request,
    const url::Origin& initiator,
    network::mojom::ReferrerPolicy policy) {
  Referrer fake_referrer(initiator.GetURL(), policy);
  Referrer sanitizied_referrer = SanitizeForRequest(request, fake_referrer);
  return url::Origin::Create(sanitizied_referrer.url);
}

// static
net::URLRequest::ReferrerPolicy Referrer::ReferrerPolicyForUrlRequest(
    network::mojom::ReferrerPolicy referrer_policy) {
  if (referrer_policy == network::mojom::ReferrerPolicy::kDefault) {
    return GetDefaultReferrerPolicy();
  }
  return network::ReferrerPolicyForUrlRequest(referrer_policy);
}

// static
network::mojom::ReferrerPolicy Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
    net::URLRequest::ReferrerPolicy net_policy) {
  switch (net_policy) {
    case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
    case net::URLRequest::
        REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
    case net::URLRequest::ORIGIN_ONLY_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
    case net::URLRequest::NEVER_CLEAR_REFERRER:
      return network::mojom::ReferrerPolicy::kAlways;
    case net::URLRequest::ORIGIN:
      return network::mojom::ReferrerPolicy::kOrigin;
    case net::URLRequest::CLEAR_REFERRER_ON_TRANSITION_CROSS_ORIGIN:
      return network::mojom::ReferrerPolicy::kSameOrigin;
    case net::URLRequest::ORIGIN_CLEAR_ON_TRANSITION_FROM_SECURE_TO_INSECURE:
      return network::mojom::ReferrerPolicy::kStrictOrigin;
    case net::URLRequest::NO_REFERRER:
      return network::mojom::ReferrerPolicy::kNever;
  }
  NOTREACHED();
  return network::mojom::ReferrerPolicy::kDefault;
}

// static
net::URLRequest::ReferrerPolicy Referrer::GetDefaultReferrerPolicy() {
  // The ReducedReferrerGranularity feature sets the default referrer
  // policy to strict-origin-when-cross-origin unless forbidden
  // by the "force legacy policy" global.
  // TODO(M82, crbug.com/1016541) Once the pertinent enterprise policy has
  // been removed, update this to remove the global.

  // Short-circuit to avoid acquiring the lock unless necessary.
  if (!base::FeatureList::IsEnabled(features::kReducedReferrerGranularity))
    return net::URLRequest::
        CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE;

  return ShouldForceLegacyDefaultReferrerPolicy()
             ? net::URLRequest::
                   CLEAR_REFERRER_ON_TRANSITION_FROM_SECURE_TO_INSECURE
             : net::URLRequest::
                   REDUCE_REFERRER_GRANULARITY_ON_TRANSITION_CROSS_ORIGIN;
}

// static
void Referrer::SetForceLegacyDefaultReferrerPolicy(bool force) {
  ReadModifyWriteForceLegacyPolicyFlag(force);
}

// static
bool Referrer::ShouldForceLegacyDefaultReferrerPolicy() {
  return ReadModifyWriteForceLegacyPolicyFlag(base::nullopt);
}

// static
network::mojom::ReferrerPolicy Referrer::ConvertToPolicy(int32_t policy) {
  return mojo::ConvertIntToMojoEnum<network::mojom::ReferrerPolicy>(policy)
      .value_or(network::mojom::ReferrerPolicy::kDefault);
}

}  // namespace content
