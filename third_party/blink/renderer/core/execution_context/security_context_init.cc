// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/security_context_init.h"

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/sandbox_flags.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/imports/html_imports_controller.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {
namespace {

// Helper function to filter out features that are not in origin trial in
// ParsedDocumentPolicy.
DocumentPolicy::ParsedDocumentPolicy FilterByOriginTrial(
    const DocumentPolicy::ParsedDocumentPolicy& parsed_policy,
    SecurityContextInit* init) {
  DocumentPolicy::ParsedDocumentPolicy filtered_policy;
  for (auto i = parsed_policy.feature_state.begin(),
            last = parsed_policy.feature_state.end();
       i != last;) {
    if (!DisabledByOriginTrial(i->first, init))
      filtered_policy.feature_state.insert(*i);
    ++i;
  }
  for (auto i = parsed_policy.endpoint_map.begin(),
            last = parsed_policy.endpoint_map.end();
       i != last;) {
    if (!DisabledByOriginTrial(i->first, init))
      filtered_policy.endpoint_map.insert(*i);
    ++i;
  }
  return filtered_policy;
}

// Helper function: Merge the feature policy strings from HTTP headers and the
// origin policy (if any).
// Headers go first, which means that the per-page headers override the
// origin policy features.
//
// TODO(domenic): we want to treat origin policy feature policy as a single
// feature policy, not a header serialization, so it should be processed
// differently.
void MergeFeaturesFromOriginPolicy(WTF::StringBuilder& feature_policy,
                                   const WebOriginPolicy& origin_policy) {
  if (!origin_policy.feature_policy.IsNull()) {
    if (!feature_policy.IsEmpty()) {
      feature_policy.Append(',');
    }
    feature_policy.Append(origin_policy.feature_policy);
  }
}

}  // namespace

// This constructor is used for non-Document contexts (i.e., workers and tests).
// This does a simpler check than Documents to set secure_context_mode_. This
// is only sufficient until there are APIs that are available in workers or
// worklets that require a privileged context test that checks ancestors.
SecurityContextInit::SecurityContextInit(scoped_refptr<SecurityOrigin> origin,
                                         OriginTrialContext* origin_trials)
    : security_origin_(std::move(origin)),
      origin_trials_(origin_trials),
      secure_context_mode_(security_origin_ &&
                                   security_origin_->IsPotentiallyTrustworthy()
                               ? SecureContextMode::kSecureContext
                               : SecureContextMode::kInsecureContext) {}

// A helper class that allows the security context be initialized in the
// process of constructing the document.
SecurityContextInit::SecurityContextInit(const DocumentInit& initializer)
    : execution_context_(initializer.GetExecutionContext()),
      csp_(initializer.GetContentSecurityPolicy()),
      sandbox_flags_(initializer.GetSandboxFlags()),
      security_origin_(initializer.GetDocumentOrigin()) {
  // Derive possibly a new security origin that contains the agent cluster id.
  if (execution_context_) {
    security_origin_ = security_origin_->GetOriginForAgentCluster(
        execution_context_->GetAgent()->cluster_id());
  }
}

void SecurityContextInit::CountFeaturePolicyUsage(
    mojom::blink::WebFeature feature) {
  if (execution_context_)
    execution_context_->CountFeaturePolicyUsage(feature);
}

bool SecurityContextInit::FeaturePolicyFeatureObserved(
    mojom::blink::FeaturePolicyFeature feature) {
  return execution_context_ &&
         execution_context_->FeaturePolicyFeatureObserved(feature);
}

bool SecurityContextInit::FeatureEnabled(OriginTrialFeature feature) const {
  return origin_trials_->IsFeatureEnabled(feature);
}

void SecurityContextInit::CalculateDocumentPolicy(
    const DocumentPolicy::ParsedDocumentPolicy& document_policy,
    const String& report_only_document_policy_header) {
  DCHECK(origin_trials_);
  if (!RuntimeEnabledFeatures::DocumentPolicyEnabled(this))
    return;

  // Because Document-Policy http header is parsed in DocumentLoader,
  // when origin trial context is not initialized yet.
  // Needs to filter out features that are not in origin trial after
  // we have origin trial information available.
  document_policy_ = FilterByOriginTrial(document_policy, this);
  if (!document_policy_.feature_state.empty()) {
    UseCounter::Count(execution_context_, WebFeature::kDocumentPolicyHeader);
    for (const auto& policy_entry : document_policy_.feature_state) {
      UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.DocumentPolicy.Header",
                                policy_entry.first);
    }
  }

  // Handle Report-Only-Document-Policy HTTP header.
  // Console messages generated from logger are discarded, because currently
  // there is no way to output them to console.
  // Calling |Document::AddConsoleMessage| in
  // |SecurityContextInit::ApplyPendingDataToDocument| will have no effect,
  // because when the function is called, the document is not fully initialized
  // yet (|document_| field in current frame is not yet initialized yet).
  PolicyParserMessageBuffer logger("%s", /* discard_message */ true);
  base::Optional<DocumentPolicy::ParsedDocumentPolicy>
      report_only_parsed_policy = DocumentPolicyParser::Parse(
          report_only_document_policy_header, logger);
  if (report_only_parsed_policy) {
    report_only_document_policy_ =
        FilterByOriginTrial(*report_only_parsed_policy, this);
    if (!report_only_document_policy_.feature_state.empty()) {
      UseCounter::Count(execution_context_,
                        WebFeature::kDocumentPolicyReportOnlyHeader);
    }
  }
}

void SecurityContextInit::CalculateFeaturePolicy(
    LocalFrame* frame,
    bool is_view_source,
    const ResourceResponse& response,
    const base::Optional<WebOriginPolicy>& origin_policy,
    const FramePolicy& frame_policy) {
  DCHECK(origin_trials_);
  initialized_feature_policy_state_ = true;
  // If we are a HTMLViewSourceDocument we use container, header or
  // inherited policies. https://crbug.com/898688.
  if (is_view_source)
    return;

  // For a main frame, get inherited feature policy from the opener if any.
  if (frame && frame->IsMainFrame() && !frame->OpenerFeatureState().empty())
    frame_for_opener_feature_state_ = frame;

  const String& permissions_policy_header =
      RuntimeEnabledFeatures::PermissionsPolicyHeaderEnabled()
          ? response.HttpHeaderField(http_names::kPermissionsPolicy)
          : g_empty_string;
  const String& report_only_permissions_policy_header =
      RuntimeEnabledFeatures::PermissionsPolicyHeaderEnabled()
          ? response.HttpHeaderField(http_names::kPermissionsPolicyReportOnly)
          : g_empty_string;

  PolicyParserMessageBuffer feature_policy_logger(
      "Error with Feature-Policy header: ");
  PolicyParserMessageBuffer report_only_feature_policy_logger(
      "Error with Report-Only-Feature-Policy header: ");

  WTF::StringBuilder feature_policy;
  feature_policy.Append(response.HttpHeaderField(http_names::kFeaturePolicy));
  if (origin_policy.has_value())
    MergeFeaturesFromOriginPolicy(feature_policy, origin_policy.value());
  String feature_policy_header = feature_policy.ToString();
  if (!feature_policy_header.IsEmpty())
    UseCounter::Count(execution_context_, WebFeature::kFeaturePolicyHeader);

  feature_policy_header_ = FeaturePolicyParser::ParseHeader(
      feature_policy_header, permissions_policy_header, security_origin_,
      feature_policy_logger, this);

  report_only_feature_policy_header_ = FeaturePolicyParser::ParseHeader(
      response.HttpHeaderField(http_names::kFeaturePolicyReportOnly),
      report_only_permissions_policy_header, security_origin_,
      report_only_feature_policy_logger, this);

  if (!report_only_feature_policy_header_.empty()) {
    UseCounter::Count(execution_context_,
                      WebFeature::kFeaturePolicyReportOnlyHeader);
  }

  if (execution_context_) {
    for (const auto& message : feature_policy_logger.GetMessages()) {
      execution_context_->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kSecurity, message.level,
              message.content));
    }
    for (const auto& message :
         report_only_feature_policy_logger.GetMessages()) {
      execution_context_->AddConsoleMessage(
          MakeGarbageCollected<ConsoleMessage>(
              mojom::blink::ConsoleMessageSource::kSecurity, message.level,
              message.content));
    }
  }

  if (sandbox_flags_ != network::mojom::blink::WebSandboxFlags::kNone &&
      RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    // The sandbox flags might have come from CSP header or the browser; in
    // such cases the sandbox is not part of the container policy. They are
    // added to the header policy (which specifically makes sense in the case
    // of CSP sandbox).
    ApplySandboxFlagsToParsedFeaturePolicy(sandbox_flags_,
                                           feature_policy_header_);
  }

  if (frame && frame->Owner())
    container_policy_ = frame_policy.container_policy;

  // TODO(icelland): This is problematic querying sandbox flags before
  // feature policy is initialized.
  if (RuntimeEnabledFeatures::BlockingFocusWithoutUserActivationEnabled() &&
      frame && frame->Tree().Parent() &&
      (sandbox_flags_ & network::mojom::blink::WebSandboxFlags::kNavigation) !=
          network::mojom::blink::WebSandboxFlags::kNone) {
    // Enforcing the policy for sandbox frames (for context see
    // https://crbug.com/954349).
    DisallowFeatureIfNotPresent(
        mojom::blink::FeaturePolicyFeature::kFocusWithoutUserActivation,
        container_policy_);
  }

  if (frame && !frame->IsMainFrame())
    parent_frame_ = frame->Tree().Parent();
}

std::unique_ptr<FeaturePolicy>
SecurityContextInit::CreateReportOnlyFeaturePolicy() const {
  // For non-Document initialization, returns nullptr directly.
  if (!initialized_feature_policy_state_)
    return nullptr;

  // If header not present, returns nullptr directly.
  if (report_only_feature_policy_header_.empty())
    return nullptr;

  // Report-only feature policy only takes effect when it is stricter than
  // enforced feature policy, i.e. when enforced feature policy allows a feature
  // while report-only feature policy do not. In such scenario, a report-only
  // policy violation report will be generated, but the feature is still allowed
  // to be used. Since child frames cannot loosen enforced feature policy, there
  // is no need to inherit parent policy and container policy for report-only
  // feature policy. For inherited policies, the behavior is dominated by
  // enforced feature policy.
  DCHECK(security_origin_);
  std::unique_ptr<FeaturePolicy> report_only_policy =
      FeaturePolicy::CreateFromParentPolicy(nullptr /* parent_policy */,
                                            {} /* container_policy */,
                                            security_origin_->ToUrlOrigin());
  report_only_policy->SetHeaderPolicy(report_only_feature_policy_header_);
  return report_only_policy;
}

std::unique_ptr<FeaturePolicy> SecurityContextInit::CreateFeaturePolicy()
    const {
  // For non-Document initialization, returns nullptr directly.
  if (!initialized_feature_policy_state_)
    return nullptr;

  // Feature policy should either come from a parent in the case of an
  // embedded child frame, or from an opener if any when a new window is
  // created by an opener. A main frame without an opener would not have a
  // parent policy nor an opener feature state.
  DCHECK(!parent_frame_ || !frame_for_opener_feature_state_);
  std::unique_ptr<FeaturePolicy> feature_policy;
  if (!frame_for_opener_feature_state_ ||
      !RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    auto* parent_feature_policy =
        parent_frame_ ? parent_frame_->GetSecurityContext()->GetFeaturePolicy()
                      : nullptr;
    feature_policy = FeaturePolicy::CreateFromParentPolicy(
        parent_feature_policy, container_policy_,
        security_origin_->ToUrlOrigin());
  } else {
    DCHECK(!parent_frame_);
    feature_policy = FeaturePolicy::CreateWithOpenerPolicy(
        frame_for_opener_feature_state_->OpenerFeatureState(),
        security_origin_->ToUrlOrigin());
  }
  feature_policy->SetHeaderPolicy(feature_policy_header_);
  return feature_policy;
}

std::unique_ptr<DocumentPolicy> SecurityContextInit::CreateDocumentPolicy()
    const {
  return DocumentPolicy::CreateWithHeaderPolicy(document_policy_);
}

std::unique_ptr<DocumentPolicy>
SecurityContextInit::CreateReportOnlyDocumentPolicy() const {
  return report_only_document_policy_.feature_state.empty()
             ? nullptr
             : DocumentPolicy::CreateWithHeaderPolicy(
                   report_only_document_policy_);
}

void SecurityContextInit::CalculateSecureContextMode(LocalFrame* frame) {
  if (!security_origin_->IsPotentiallyTrustworthy()) {
    secure_context_mode_ = SecureContextMode::kInsecureContext;
  } else if (SchemeRegistry::SchemeShouldBypassSecureContextCheck(
                 security_origin_->Protocol())) {
    secure_context_mode_ = SecureContextMode::kSecureContext;
  } else if (frame) {
    Frame* parent = frame->Tree().Parent();
    while (parent) {
      if (!parent->GetSecurityContext()
               ->GetSecurityOrigin()
               ->IsPotentiallyTrustworthy()) {
        secure_context_mode_ = SecureContextMode::kInsecureContext;
        break;
      }
      parent = parent->Tree().Parent();
    }
    if (!secure_context_mode_.has_value())
      secure_context_mode_ = SecureContextMode::kSecureContext;
  } else {
    secure_context_mode_ = SecureContextMode::kInsecureContext;
  }
  bool is_secure = secure_context_mode_ == SecureContextMode::kSecureContext;
  if (GetSandboxFlags() != network::mojom::blink::WebSandboxFlags::kNone) {
    UseCounter::Count(
        execution_context_,
        is_secure ? WebFeature::kSecureContextCheckForSandboxedOriginPassed
                  : WebFeature::kSecureContextCheckForSandboxedOriginFailed);
  }

  UseCounter::Count(execution_context_,
                    is_secure ? WebFeature::kSecureContextCheckPassed
                              : WebFeature::kSecureContextCheckFailed);
}

void SecurityContextInit::InitializeOriginTrials(
    const String& origin_trials_header) {
  DCHECK(secure_context_mode_.has_value());
  origin_trials_ = MakeGarbageCollected<OriginTrialContext>();
  if (origin_trials_header.IsEmpty())
    return;
  std::unique_ptr<Vector<String>> tokens(
      OriginTrialContext::ParseHeaderValue(origin_trials_header));
  if (!tokens)
    return;
  origin_trials_->AddTokens(
      *tokens, security_origin_.get(),
      secure_context_mode_ == SecureContextMode::kSecureContext);
}

}  // namespace blink
