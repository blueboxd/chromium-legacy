// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"

#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

TrustedTypePolicy* TrustedTypePolicyFactory::createPolicy(
    const String& policy_name,
    const TrustedTypePolicyOptions* policy_options,
    bool exposed,
    ExceptionState& exception_state) {
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kTrustedTypesCreatePolicy);
  if (RuntimeEnabledFeatures::TrustedDOMTypesEnabled(GetExecutionContext()) &&
      !GetExecutionContext()
           ->GetContentSecurityPolicy()
           ->AllowTrustedTypePolicy(policy_name)) {
    exception_state.ThrowTypeError("Policy " + policy_name + " disallowed.");
    return nullptr;
  }
  // TODO(orsibatiz): After policy naming rules are estabilished, check for the
  // policy_name to be according to them.
  if (policy_map_.Contains(policy_name)) {
    exception_state.ThrowTypeError("Policy with name " + policy_name +
                                   " already exists.");
    return nullptr;
  }
  if (policy_name == "default" && !exposed) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The default policy must be exposed.");
    return nullptr;
  }
  if (policy_name == "default") {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kTrustedTypesDefaultPolicyUsed);
  }
  auto* policy = MakeGarbageCollected<TrustedTypePolicy>(
      policy_name, const_cast<TrustedTypePolicyOptions*>(policy_options),
      exposed);
  policy_map_.insert(policy_name, policy);
  return policy;
}

TrustedTypePolicy* TrustedTypePolicyFactory::getExposedPolicy(
    const String& policy_name) {
  TrustedTypePolicy* p = policy_map_.at(policy_name);
  if (p && p->exposed()) {
    return p;
  }
  return nullptr;
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(ExecutionContext* context)
    : ContextClient(context),
      empty_html_(MakeGarbageCollected<TrustedHTML>("")) {
  UseCounter::Count(context, WebFeature::kTrustedTypesEnabled);
}

Vector<String> TrustedTypePolicyFactory::getPolicyNames() const {
  Vector<String> policyNames;
  for (const String name : policy_map_.Keys()) {
    policyNames.push_back(name);
  }
  return policyNames;
}

const WrapperTypeInfo*
TrustedTypePolicyFactory::GetWrapperTypeInfoFromScriptValue(
    ScriptState* script_state,
    const ScriptValue& script_value) {
  v8::Local<v8::Value> value = script_value.V8Value();
  if (value.IsEmpty() || !value->IsObject() ||
      !V8DOMWrapper::IsWrapper(script_state->GetIsolate(), value))
    return nullptr;
  v8::Local<v8::Object> object = value.As<v8::Object>();
  return ToWrapperTypeInfo(object);
}

bool TrustedTypePolicyFactory::isHTML(ScriptState* script_state,
                                      const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedHTML::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isScript(ScriptState* script_state,
                                        const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedScript::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isScriptURL(ScriptState* script_state,
                                           const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedScriptURL::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isURL(ScriptState* script_state,
                                     const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedURL::GetWrapperTypeInfo());
}

TrustedHTML* TrustedTypePolicyFactory::emptyHTML() const {
  return empty_html_.Get();
}

void TrustedTypePolicyFactory::CountTrustedTypeAssignmentError() {
  if (!hadAssignmentError) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kTrustedTypesAssignmentError);
    hadAssignmentError = true;
  }
}

void TrustedTypePolicyFactory::Trace(blink::Visitor* visitor) {
  ScriptWrappable::Trace(visitor);
  ContextClient::Trace(visitor);
  visitor->Trace(empty_html_);
  visitor->Trace(policy_map_);
}

}  // namespace blink
