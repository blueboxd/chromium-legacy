// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/execution_context/security_context_init.h"

#include "base/metrics/histogram_macros.h"
#include "services/network/public/cpp/web_sandbox_flags.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/window_agent.h"
#include "third_party/blink/renderer/core/execution_context/window_agent_factory.h"
#include "third_party/blink/renderer/core/feature_policy/document_policy_parser.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
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
#include "third_party/blink/renderer/platform/web_test_support.h"

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

}  // namespace

// This is the constructor used by RemoteSecurityContext
SecurityContextInit::SecurityContextInit()
    : SecurityContextInit(nullptr, nullptr, nullptr) {}

// This constructor is used for non-Document contexts (i.e., workers and tests).
// This does a simpler check than Documents to set secure_context_mode_. This
// is only sufficient until there are APIs that are available in workers or
// worklets that require a privileged context test that checks ancestors.
SecurityContextInit::SecurityContextInit(scoped_refptr<SecurityOrigin> origin,
                                         OriginTrialContext* origin_trials,
                                         Agent* agent)
    : security_origin_(std::move(origin)),
      origin_trials_(origin_trials),
      agent_(agent),
      secure_context_mode_(security_origin_ &&
                                   security_origin_->IsPotentiallyTrustworthy()
                               ? SecureContextMode::kSecureContext
                               : SecureContextMode::kInsecureContext) {}

// A helper class that allows the security context be initialized in the
// process of constructing the document.
SecurityContextInit::SecurityContextInit(const DocumentInit& initializer) {
  // Content Security Policy can provide sandbox flags. In CSP
  // 'self' will be determined when the policy is bound. That occurs
  // once the document is constructed.
  InitializeContentSecurityPolicy(initializer);

  // Sandbox flags can come from initializer, loader or CSP.
  InitializeSandboxFlags(initializer);

  // The origin can be opaque based on sandbox flags.
  InitializeOrigin(initializer);

  // The secure context state is based on the origin.
  InitializeSecureContextMode(initializer);

  // Initialize origin trials, requires the post sandbox flags
  // security origin and secure context state.
  InitializeOriginTrials(initializer);

  // Initialize feature policy, depends on origin trials.
  InitializeFeaturePolicy(initializer);

  // Initialize document policy.
  InitializeDocumentPolicy(initializer);

  // Initialize the agent. Depends on security origin.
  InitializeAgent(initializer);
}

bool SecurityContextInit::FeaturePolicyFeatureObserved(
    mojom::blink::FeaturePolicyFeature feature) {
  if (parsed_feature_policies_.Contains(feature))
    return true;
  parsed_feature_policies_.insert(feature);
  return false;
}

bool SecurityContextInit::FeatureEnabled(OriginTrialFeature feature) const {
  return origin_trials_->IsFeatureEnabled(feature);
}

void SecurityContextInit::ApplyPendingDataToDocument(Document& document) const {
  for (auto feature : feature_count_)
    UseCounter::Count(document, feature);
  for (auto feature : parsed_feature_policies_)
    document.GetExecutionContext()->FeaturePolicyFeatureObserved(feature);
  for (const auto& message : feature_policy_parse_messages_) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "Error with Feature-Policy header: " + message));
  }
  for (const auto& message : report_only_feature_policy_parse_messages_) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kSecurity,
        mojom::blink::ConsoleMessageLevel::kError,
        "Error with Feature-Policy-Report-Only header: " + message));
  }
  if (!report_only_feature_policy_header_.empty())
    UseCounter::Count(document, WebFeature::kFeaturePolicyReportOnlyHeader);

  if (!document_policy_.feature_state.empty())
    UseCounter::Count(document, WebFeature::kDocumentPolicyHeader);

  if (!report_only_document_policy_.feature_state.empty())
    UseCounter::Count(document, WebFeature::kDocumentPolicyReportOnlyHeader);

  for (const auto& policy_entry : document_policy_.feature_state) {
    UMA_HISTOGRAM_ENUMERATION("Blink.UseCounter.DocumentPolicy.Header",
                              policy_entry.first);
  }
}

void SecurityContextInit::InitializeContentSecurityPolicy(
    const DocumentInit& initializer) {
  // --------------
  // THE MAIN PATH:
  // --------------
  //
  // This path is used (among others) to load a document inside a frame. In this
  // case, the CSP is computed by:
  // - FrameLoader::CreateCSPForInitialEmptyDocument() or
  // - FrameLoader::CreateCSP().
  csp_ = initializer.GetContentSecurityPolicy();
  if (csp_)
    return;

  // A few users of DocumentInit do not specify the CSP to be used. An empty CSP
  // is used in this case.
  // TODO(arthursonzogni): Audit every users of DocumentInit::Create() that do
  // not specify a CSP to be used. Ideally they should be forced to explicitly
  // choose one.
  csp_ = MakeGarbageCollected<ContentSecurityPolicy>();
  bind_csp_immediately_ = true;
}

void SecurityContextInit::InitializeSandboxFlags(
    const DocumentInit& initializer) {
  sandbox_flags_ = initializer.GetSandboxFlags() | csp_->GetSandboxMask();
  auto* frame = initializer.GetFrame();
  if (frame && frame->Loader().GetDocumentLoader()->Archive()) {
    // The URL of a Document loaded from a MHTML archive is controlled by
    // the Content-Location header. This would allow UXSS, since
    // Content-Location can be arbitrarily controlled to control the
    // Document's URL and origin. Instead, force a Document loaded from a
    // MHTML archive to be sandboxed, providing exceptions only for creating
    // new windows.
    sandbox_flags_ |= (network::mojom::blink::WebSandboxFlags::kAll &
                       ~(network::mojom::blink::WebSandboxFlags::kPopups |
                         network::mojom::blink::WebSandboxFlags::
                             kPropagatesToAuxiliaryBrowsingContexts));
  }
}

void SecurityContextInit::InitializeOrigin(const DocumentInit& initializer) {
  scoped_refptr<SecurityOrigin> document_origin =
      initializer.GetDocumentOrigin();
  if ((sandbox_flags_ & network::mojom::blink::WebSandboxFlags::kOrigin) !=
      network::mojom::blink::WebSandboxFlags::kNone) {
    scoped_refptr<SecurityOrigin> sandboxed_origin =
        initializer.OriginToCommit() ? initializer.OriginToCommit()
                                     : document_origin->DeriveNewOpaqueOrigin();

    // If we're supposed to inherit our security origin from our
    // owner, but we're also sandboxed, the only things we inherit are
    // the origin's potential trustworthiness and the ability to
    // load local resources. The latter lets about:blank iframes in
    // file:// URL documents load images and other resources from
    // the file system.
    //
    // Note: Sandboxed about:srcdoc iframe without "allow-same-origin" aren't
    // allowed to load user's file, even if its parent can.
    if (initializer.OwnerDocument()) {
      if (document_origin->IsPotentiallyTrustworthy())
        sandboxed_origin->SetOpaqueOriginIsPotentiallyTrustworthy(true);
      if (document_origin->CanLoadLocalResources() &&
          !initializer.IsSrcdocDocument())
        sandboxed_origin->GrantLoadLocalResources();
    }
    security_origin_ = sandboxed_origin;
  } else {
    security_origin_ = document_origin;
  }

  // If we are a page popup in LayoutTests ensure we use the popup
  // owner's security origin so the tests can possibly access the
  // document via internals API.
  auto* frame = initializer.GetFrame();
  if (frame && frame->GetPage()->GetChromeClient().IsPopup() &&
      WebTestSupport::IsRunningWebTest()) {
    security_origin_ = frame->PagePopupOwner()
                           ->GetDocument()
                           .GetSecurityOrigin()
                           ->IsolatedCopy();
  }

  if (initializer.HasSecurityContext()) {
    if (Settings* settings = initializer.GetSettings()) {
      if (!settings->GetWebSecurityEnabled()) {
        // Web security is turned off. We should let this document access
        // every other document. This is used primary by testing harnesses for
        // web sites.
        security_origin_->GrantUniversalAccess();
      } else if (security_origin_->IsLocal()) {
        if (settings->GetAllowUniversalAccessFromFileURLs()) {
          // Some clients want local URLs to have universal access, but that
          // setting is dangerous for other clients.
          security_origin_->GrantUniversalAccess();
        } else if (!settings->GetAllowFileAccessFromFileURLs()) {
          // Some clients do not want local URLs to have access to other local
          // URLs.
          security_origin_->BlockLocalAccessFromLocalOrigin();
        }
      }
    }
  }

  if (initializer.GrantLoadLocalResources())
    security_origin_->GrantLoadLocalResources();

  if (security_origin_->IsOpaque() && initializer.ShouldSetURL()) {
    KURL url = initializer.Url().IsEmpty() ? BlankURL() : initializer.Url();
    if (SecurityOrigin::Create(url)->IsPotentiallyTrustworthy())
      security_origin_->SetOpaqueOriginIsPotentiallyTrustworthy(true);
  }
}

void SecurityContextInit::InitializeDocumentPolicy(
    const DocumentInit& initializer) {
  if (!RuntimeEnabledFeatures::DocumentPolicyEnabled(this))
    return;

  // Because Document-Policy http header is parsed in DocumentLoader,
  // when origin trial context is not initialized yet.
  // Needs to filter out features that are not in origin trial after
  // we have origin trial information available.
  document_policy_ = FilterByOriginTrial(initializer.GetDocumentPolicy(), this);

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
          initializer.ReportOnlyDocumentPolicyHeader(), logger);
  if (report_only_parsed_policy) {
    report_only_document_policy_ =
        FilterByOriginTrial(*report_only_parsed_policy, this);
  }
}

void SecurityContextInit::InitializeFeaturePolicy(
    const DocumentInit& initializer) {
  initialized_feature_policy_state_ = true;
  // If we are a HTMLViewSourceDocument we use container, header or
  // inherited policies. https://crbug.com/898688. Don't set any from the
  // initializer or frame below.
  if (initializer.GetType() == DocumentInit::Type::kViewSource)
    return;

  auto* frame = initializer.GetFrame();
  // For a main frame, get inherited feature policy from the opener if any.
  if (frame && frame->IsMainFrame() && !frame->OpenerFeatureState().empty())
    frame_for_opener_feature_state_ = frame;

  feature_policy_header_ = FeaturePolicyParser::ParseHeader(
      initializer.FeaturePolicyHeader(), security_origin_,
      &feature_policy_parse_messages_, this);

  report_only_feature_policy_header_ = FeaturePolicyParser::ParseHeader(
      initializer.ReportOnlyFeaturePolicyHeader(), security_origin_,
      &report_only_feature_policy_parse_messages_, this);

  if (sandbox_flags_ != network::mojom::blink::WebSandboxFlags::kNone &&
      RuntimeEnabledFeatures::FeaturePolicyForSandboxEnabled()) {
    // The sandbox flags might have come from CSP header or the browser; in
    // such cases the sandbox is not part of the container policy. They are
    // added to the header policy (which specifically makes sense in the case
    // of CSP sandbox).
    ApplySandboxFlagsToParsedFeaturePolicy(sandbox_flags_,
                                           feature_policy_header_);
  }

  if (frame && frame->Owner()) {
    container_policy_ =
        initializer.GetFramePolicy().value_or(FramePolicy()).container_policy;
  }

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

void SecurityContextInit::InitializeSecureContextMode(
    const DocumentInit& initializer) {
  auto* frame = initializer.GetFrame();
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
    feature_count_.insert(
        is_secure ? WebFeature::kSecureContextCheckForSandboxedOriginPassed
                  : WebFeature::kSecureContextCheckForSandboxedOriginFailed);
  }
  feature_count_.insert(is_secure ? WebFeature::kSecureContextCheckPassed
                                  : WebFeature::kSecureContextCheckFailed);
}

void SecurityContextInit::InitializeOriginTrials(
    const DocumentInit& initializer) {
  DCHECK(secure_context_mode_.has_value());
  origin_trials_ = MakeGarbageCollected<OriginTrialContext>();

  const String& header_value = initializer.OriginTrialsHeader();

  if (header_value.IsEmpty())
    return;
  std::unique_ptr<Vector<String>> tokens(
      OriginTrialContext::ParseHeaderValue(header_value));
  if (!tokens)
    return;
  origin_trials_->AddTokens(
      security_origin_.get(),
      secure_context_mode_ == SecureContextMode::kSecureContext, *tokens);
}

void SecurityContextInit::InitializeAgent(const DocumentInit& initializer) {
  // If we are allowed to share our document with other windows then we need
  // to look at the window agent factory, otherwise we should create our own
  // window agent.
  if (auto* window_agent_factory = initializer.GetWindowAgentFactory()) {
    bool has_potential_universal_access_privilege = false;
    if (auto* settings = initializer.GetSettingsForWindowAgentFactory()) {
      // TODO(keishi): Also check if AllowUniversalAccessFromFileURLs might
      // dynamically change.
      if (!settings->GetWebSecurityEnabled() ||
          settings->GetAllowUniversalAccessFromFileURLs())
        has_potential_universal_access_privilege = true;
    }
    agent_ = window_agent_factory->GetAgentForOrigin(
        has_potential_universal_access_privilege,
        V8PerIsolateData::MainThreadIsolate(), security_origin_.get());
  } else {
    // ContextDocument is null only for Documents created in unit tests.
    // In that case, use a throw away WindowAgent.
    agent_ = MakeGarbageCollected<WindowAgent>(
        V8PerIsolateData::MainThreadIsolate());
  }

  // Derive possibly a new security origin that contains the cluster id.
  security_origin_ =
      security_origin_->GetOriginForAgentCluster(agent_->cluster_id());
}

}  // namespace blink
