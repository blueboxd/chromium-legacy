/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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

#include "third_party/blink/renderer/core/execution_context/security_context.h"

#include "base/metrics/histogram_macros.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/security_context_init.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

namespace {

// Bucketize image metrics into percentage in the following fashion:
// if an image's metrics is 0.1, it will be represented as 1 percent
// if an image's metrics is 5, it will be represented as 50 percents.
int BucketizeImageMetrics(double ratio) {
  int ratio_percent = 10 * ratio;
  if (ratio_percent < 0)
    return 0;
  return ratio_percent > 100 ? 100 : ratio_percent;
}

inline const char* GetImagePolicyHistogramName(
    mojom::blink::FeaturePolicyFeature feature) {
  switch (feature) {
    case mojom::blink::FeaturePolicyFeature::kUnoptimizedLossyImages:
      return "Blink.UseCounter.FeaturePolicy.LossyImageCompression";
    case mojom::blink::FeaturePolicyFeature::kUnoptimizedLosslessImages:
      return "Blink.UseCounter.FeaturePolicy.LosslessImageCompression";
    case mojom::blink::FeaturePolicyFeature::kUnoptimizedLosslessImagesStrict:
      return "Blink.UseCounter.FeaturePolicy.StrictLosslessImageCompression";
    case mojom::blink::FeaturePolicyFeature::kOversizedImages:
      return "Blink.UseCounter.FeaturePolicy.ImageDownscalingRatio";
    default:
      NOTREACHED();
      break;
  }
  return "";
}

}  // namespace

// static
WTF::Vector<unsigned> SecurityContext::SerializeInsecureNavigationSet(
    const InsecureNavigationsSet& set) {
  // The set is serialized as a sorted array. Sorting it makes it easy to know
  // if two serialized sets are equal.
  WTF::Vector<unsigned> serialized;
  serialized.ReserveCapacity(set.size());
  for (unsigned host : set)
    serialized.emplace_back(host);
  std::sort(serialized.begin(), serialized.end());

  return serialized;
}

SecurityContext::SecurityContext(const SecurityContextInit& init,
                                 SecurityContextType context_type)
    : sandbox_flags_(init.GetSandboxFlags()),
      security_origin_(init.GetSecurityOrigin()),
      feature_policy_(init.CreateFeaturePolicy()),
      document_policy_(init.CreateDocumentPolicy()),
      content_security_policy_(init.GetCSP()),
      address_space_(network::mojom::IPAddressSpace::kUnknown),
      insecure_request_policy_(kLeaveInsecureRequestsAlone),
      require_safe_types_(false),
      context_type_(context_type) {}

void SecurityContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(content_security_policy_);
}

void SecurityContext::SetSecurityOrigin(
    scoped_refptr<SecurityOrigin> security_origin) {
  // Enforce that we don't change access, we might change the reference (via
  // IsolatedCopy but we can't change the security policy).
  CHECK(security_origin);
  // The purpose of this check is to ensure that the SecurityContext does not
  // change after script has executed in the ExecutionContext. If this is a
  // RemoteSecurityContext, then there is no local script execution, so it is
  // safe for the SecurityOrigin to change.
  // Exempting null ExecutionContexts is also necessary because RemoteFrames and
  // RemoteSecurityContexts do not change when a cross-origin navigation happens
  // remotely.
  CHECK(context_type_ == kRemote || !security_origin_ ||
        security_origin_->CanAccess(security_origin.get()));
  security_origin_ = std::move(security_origin);
}

void SecurityContext::SetSecurityOriginForTesting(
    scoped_refptr<SecurityOrigin> security_origin) {
  security_origin_ = std::move(security_origin);
}

void SecurityContext::SetContentSecurityPolicy(
    ContentSecurityPolicy* content_security_policy) {
  content_security_policy_ = content_security_policy;
}

bool SecurityContext::IsSandboxed(WebSandboxFlags mask) const {
  if (RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    mojom::blink::FeaturePolicyFeature feature =
        FeaturePolicy::FeatureForSandboxFlag(mask);
    if (feature != mojom::blink::FeaturePolicyFeature::kNotFound)
      return !feature_policy_->IsFeatureEnabled(feature);
  }
  return (sandbox_flags_ & mask) != WebSandboxFlags::kNone;
}

void SecurityContext::SetRequireTrustedTypes() {
  DCHECK(require_safe_types_ ||
         content_security_policy_->IsRequireTrustedTypes());
  require_safe_types_ = true;
}

void SecurityContext::SetRequireTrustedTypesForTesting() {
  require_safe_types_ = true;
}

bool SecurityContext::TrustedTypesRequiredByPolicy() const {
  return require_safe_types_;
}

void SecurityContext::SetFeaturePolicy(
    std::unique_ptr<FeaturePolicy> feature_policy) {
  // This method should be called before a FeaturePolicy has been created.
  DCHECK(!feature_policy_);
  feature_policy_ = std::move(feature_policy);
}

// Uses the parent enforcing policy as the basis for the report-only policy.
void SecurityContext::AddReportOnlyFeaturePolicy(
    const ParsedFeaturePolicy& parsed_report_only_header,
    const ParsedFeaturePolicy& container_policy,
    const FeaturePolicy* parent_feature_policy) {
  report_only_feature_policy_ = FeaturePolicy::CreateFromParentPolicy(
      parent_feature_policy, container_policy, security_origin_->ToUrlOrigin());
  report_only_feature_policy_->SetHeaderPolicy(parsed_report_only_header);
}

void SecurityContext::SetDocumentPolicyForTesting(
    std::unique_ptr<DocumentPolicy> document_policy) {
  document_policy_ = std::move(document_policy);
}

bool SecurityContext::IsFeatureEnabled(
    mojom::blink::FeaturePolicyFeature feature) const {
  return IsFeatureEnabled(
      feature, PolicyValue::CreateMaxPolicyValue(
                   feature_policy_->GetFeatureList().at(feature).second));
}

bool SecurityContext::IsFeatureEnabled(
    mojom::blink::FeaturePolicyFeature feature,
    PolicyValue threshold_value,
    base::Optional<mojom::FeaturePolicyDisposition>* disposition) const {
  // Use Document Policy to determine feature availability, but only if all of
  // the following are true:
  // * The DocumentPolicy RuntimeEnabledFeature is not disabled,
  // * Document policy has been set on this object, and
  // * Document policy infrastructure actually supports the feature.
  // If any of those are false, assume true (enabled) here. Otherwise, check
  // this object's policy.
  bool document_policy_result =
      !RuntimeEnabledFeatures::DocumentPolicyEnabled() || !document_policy_ ||
      !document_policy_->IsFeatureSupported(feature) ||
      document_policy_->IsFeatureEnabled(feature, threshold_value);

  FeatureEnabledState state = GetFeatureEnabledState(feature, threshold_value);
  if (state == FeatureEnabledState::kEnabled)
    return document_policy_result;
  if (disposition) {
    *disposition = (state == FeatureEnabledState::kReportOnly)
                       ? mojom::FeaturePolicyDisposition::kReport
                       : mojom::FeaturePolicyDisposition::kEnforce;
  }
  return (state != FeatureEnabledState::kDisabled) && document_policy_result;
}

FeatureEnabledState SecurityContext::GetFeatureEnabledState(
    mojom::blink::FeaturePolicyFeature feature,
    PolicyValue threshold_value) const {
  // The policy should always be initialized before checking it to ensure we
  // properly inherit the parent policy.
  DCHECK(feature_policy_);

  // Log metrics for unoptimized-*-images and oversized-images policies.
  if ((feature >= mojom::blink::FeaturePolicyFeature::kUnoptimizedLossyImages &&
       feature <= mojom::blink::FeaturePolicyFeature::
                      kUnoptimizedLosslessImagesStrict) ||
      feature == mojom::blink::FeaturePolicyFeature::kOversizedImages) {
    // Only log metrics if an image policy is specified.
    // If an image policy is specified, the policy value would be less than the
    // max value, otherwise by default the policy value is set to be the max
    // value.
    const auto max_value =
        PolicyValue::CreateMaxPolicyValue(mojom::PolicyValueType::kDecDouble);
    if (!feature_policy_->IsFeatureEnabled(feature, max_value) &&
        threshold_value < max_value) {
      STATIC_HISTOGRAM_POINTER_GROUP(
          GetImagePolicyHistogramName(feature), static_cast<int>(feature),
          static_cast<int>(mojom::blink::FeaturePolicyFeature::
                               kUnoptimizedLosslessImagesStrict) +
              1,
          Add(BucketizeImageMetrics(threshold_value.DoubleValue())),
          base::LinearHistogram::FactoryGet(
              GetImagePolicyHistogramName(feature), 0, 100, 101, 0x1));
    }
  }
  if (feature_policy_->IsFeatureEnabled(feature, threshold_value)) {
    if (report_only_feature_policy_ &&
        !report_only_feature_policy_->IsFeatureEnabled(feature,
                                                       threshold_value)) {
      return FeatureEnabledState::kReportOnly;
    }
    return FeatureEnabledState::kEnabled;
  }
  return FeatureEnabledState::kDisabled;
}

}  // namespace blink
