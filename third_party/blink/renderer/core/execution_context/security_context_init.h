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
class Frame;
class LocalFrame;
class OriginTrialContext;
class SecurityOrigin;
enum class WebSandboxFlags;

class CORE_EXPORT SecurityContextInit : public FeaturePolicyParserDelegate {
  STACK_ALLOCATED();

 public:
  SecurityContextInit() = default;
  SecurityContextInit(scoped_refptr<SecurityOrigin>,
                      OriginTrialContext*,
                      Agent*);
  explicit SecurityContextInit(const DocumentInit&);

  const scoped_refptr<SecurityOrigin>& GetSecurityOrigin() const {
    return security_origin_;
  }

  WebSandboxFlags GetSandboxFlags() const { return sandbox_flags_; }

  ContentSecurityPolicy* GetCSP() const { return csp_; }

  std::unique_ptr<FeaturePolicy> CreateFeaturePolicy() const;

  std::unique_ptr<DocumentPolicy> CreateDocumentPolicy() const;

  const ParsedFeaturePolicy& FeaturePolicyHeader() const {
    return feature_policy_header_;
  }

  OriginTrialContext* GetOriginTrialContext() const { return origin_trials_; }

  Agent* GetAgent() const { return agent_; }
  SecureContextMode GetSecureContextMode() const {
    return secure_context_mode_.value();
  }

  void CountFeaturePolicyUsage(mojom::WebFeature feature) override {
    feature_count_.insert(feature);
  }

  bool FeaturePolicyFeatureObserved(
      mojom::blink::FeaturePolicyFeature) override;
  bool FeatureEnabled(OriginTrialFeature feature) const override;

  void ApplyPendingDataToDocument(Document&) const;

  bool BindCSPImmediately() const { return bind_csp_immediately_; }

 private:
  void InitializeContentSecurityPolicy(const DocumentInit&);
  void InitializeOrigin(const DocumentInit&);
  void InitializeSandboxFlags(const DocumentInit&);
  void InitializeFeaturePolicy(const DocumentInit&);
  void InitializeSecureContextMode(const DocumentInit&);
  void InitializeOriginTrials(const DocumentInit&);
  void InitializeAgent(const DocumentInit&);

  scoped_refptr<SecurityOrigin> security_origin_;
  WebSandboxFlags sandbox_flags_ = WebSandboxFlags::kNone;
  base::Optional<DocumentPolicy::FeatureState> document_policy_;
  bool initialized_feature_policy_state_ = false;
  Vector<String> feature_policy_parse_messages_;
  ParsedFeaturePolicy feature_policy_header_;
  LocalFrame* frame_for_opener_feature_state_ = nullptr;
  Frame* parent_frame_ = nullptr;
  ParsedFeaturePolicy container_policy_;
  ContentSecurityPolicy* csp_ = nullptr;
  OriginTrialContext* origin_trials_ = nullptr;
  Agent* agent_ = nullptr;
  HashSet<mojom::blink::FeaturePolicyFeature> parsed_feature_policies_;
  HashSet<mojom::WebFeature> feature_count_;
  bool bind_csp_immediately_ = false;
  base::Optional<SecureContextMode> secure_context_mode_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXECUTION_CONTEXT_SECURITY_CONTEXT_INIT_H_
