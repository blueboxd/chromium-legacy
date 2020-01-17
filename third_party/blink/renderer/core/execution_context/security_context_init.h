// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_

#include "third_party/blink/public/common/feature_policy/feature_policy.h"
#include "third_party/blink/public/mojom/feature_policy/feature_policy_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/feature_policy/feature_policy_parser_delegate.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {
class Agent;
class ContentSecurityPolicy;
class Document;
class DocumentInit;
class OriginTrialContext;
class SecurityOrigin;
class SecurityOrigin;
enum class WebSandboxFlags;
class WindowAgentFactory;

using DocumentClassFlags = unsigned char;

class SecurityContextInit : public FeaturePolicyParserDelegate {
  STACK_ALLOCATED();

 public:
  explicit SecurityContextInit(const DocumentInit&, DocumentClassFlags);

  const scoped_refptr<SecurityOrigin>& GetSecurityOrigin() const {
    return security_origin_;
  }

  WebSandboxFlags GetSandboxFlags() { return sandbox_flags_; }

  ContentSecurityPolicy* GetCSP() const { return csp_; }

  std::unique_ptr<FeaturePolicy> TakeFeaturePolicy() {
    DCHECK(feature_policy_);
    return std::move(feature_policy_);
  }

  std::unique_ptr<DocumentPolicy> TakeDocumentPolicy() {
    DCHECK(document_policy_);
    return std::move(document_policy_);
  }

  const Vector<String>& FeaturePolicyParseMessages() const {
    return feature_policy_parse_messages_;
  }
  const ParsedFeaturePolicy& ParsedHeader() const { return parsed_header_; }

  OriginTrialContext* GetOriginTrialContext() { return origin_trials_; }

  WindowAgentFactory* GetWindowAgentFactory() { return window_agent_factory_; }
  Agent* GetAgent() { return agent_; }
  SecureContextMode GetSecureContextMode() {
    return secure_context_mode_.value();
  }

  void CountFeaturePolicyUsage(mojom::WebFeature feature) override {
    feature_count_.insert(feature);
  }

  bool FeaturePolicyFeatureObserved(mojom::FeaturePolicyFeature) override;
  bool FeatureEnabled(OriginTrialFeature feature) const override;

  void ApplyPendingDataToDocument(Document&);

  bool BindCSPImmediately() const { return bind_csp_immediately_; }

 private:
  void InitializeContentSecurityPolicy(const DocumentInit&, DocumentClassFlags);
  void InitializeOrigin(const DocumentInit&);
  void InitializeSandboxFlags(const DocumentInit&);
  void InitializeFeaturePolicy(const DocumentInit&, DocumentClassFlags);
  void InitializeSecureContextMode(const DocumentInit&);
  void InitializeOriginTrials(const DocumentInit&);
  void InitializeAgent(const DocumentInit&);

  scoped_refptr<SecurityOrigin> security_origin_;
  WebSandboxFlags sandbox_flags_ = WebSandboxFlags::kNone;
  std::unique_ptr<FeaturePolicy> feature_policy_;
  std::unique_ptr<DocumentPolicy> document_policy_;
  Vector<String> feature_policy_parse_messages_;
  ParsedFeaturePolicy parsed_header_;
  Member<ContentSecurityPolicy> csp_;
  Member<OriginTrialContext> origin_trials_;
  Member<Agent> agent_;
  Member<WindowAgentFactory> window_agent_factory_;
  HashSet<mojom::FeaturePolicyFeature> parsed_feature_policies_;
  HashSet<mojom::WebFeature> feature_count_;
  bool bind_csp_immediately_ = false;
  base::Optional<SecureContextMode> secure_context_mode_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_
