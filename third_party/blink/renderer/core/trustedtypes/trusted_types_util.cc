// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"

#include "third_party/blink/public/mojom/reporting/reporting.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_html_or_trusted_script_or_trusted_script_url.h"
#include "third_party/blink/renderer/bindings/core/v8/string_or_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/window_proxy_manager.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

enum TrustedTypeViolationKind {
  kTrustedHTMLAssignment,
  kTrustedScriptAssignment,
  kTrustedScriptURLAssignment,
  kTrustedHTMLAssignmentAndDefaultPolicyFailed,
  kTrustedHTMLAssignmentAndNoDefaultPolicyExisted,
  kTrustedScriptAssignmentAndDefaultPolicyFailed,
  kTrustedScriptAssignmentAndNoDefaultPolicyExisted,
  kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
  kTrustedScriptURLAssignmentAndNoDefaultPolicyExisted,
  kNavigateToJavascriptURL,
  kNavigateToJavascriptURLAndDefaultPolicyFailed,
  kScriptExecution,
  kScriptExecutionAndDefaultPolicyFailed,
};

const char* GetMessage(TrustedTypeViolationKind kind) {
  switch (kind) {
    case kTrustedHTMLAssignment:
      return "This document requires 'TrustedHTML' assignment.";
    case kTrustedScriptAssignment:
      return "This document requires 'TrustedScript' assignment.";
    case kTrustedScriptURLAssignment:
      return "This document requires 'TrustedScriptURL' assignment.";
    case kTrustedHTMLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedHTML' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedHTMLAssignmentAndNoDefaultPolicyExisted:
      return "This document requires 'TrustedHTML' assignment and no "
             "'default' policy for 'TrustedHTML' has been defined.";
    case kTrustedScriptAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptAssignmentAndNoDefaultPolicyExisted:
      return "This document requires 'TrustedScript' assignment and no "
             "'default' policy for 'TrustedScript' has been defined.";
    case kTrustedScriptURLAssignmentAndDefaultPolicyFailed:
      return "This document requires 'TrustedScriptURL' assignment and the "
             "'default' policy failed to execute.";
    case kTrustedScriptURLAssignmentAndNoDefaultPolicyExisted:
      return "This document requires 'TrustedScriptURL' assignment and no "
             "'default' policy for 'TrustedScriptURL' has been defined.";
    case kNavigateToJavascriptURL:
      return "This document requires 'TrustedScript' assignment. "
             "Navigating to a javascript:-URL is equivalent to a "
             "'TrustedScript' assignment.";
    case kNavigateToJavascriptURLAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment. "
             "Navigating to a javascript:-URL is equivalent to a "
             "'TrustedScript' assignment and the 'default' policy failed to"
             "execute.";
    case kScriptExecution:
      return "This document requires 'TrustedScript' assignment. "
             "This script element was modified without use of TrustedScript "
             "assignment.";
    case kScriptExecutionAndDefaultPolicyFailed:
      return "This document requires 'TrustedScript' assignment. "
             "This script element was modified without use of TrustedScript "
             "assignment and the 'default' policy failed to execute.";
  }
  NOTREACHED();
  return "";
}

String GetSamplePrefix(const ExceptionState& exception_state) {
  const char* interface_name = exception_state.InterfaceName();
  const char* property_name = exception_state.PropertyName();

  // We have two sample formats, one for eval and one for assignment.
  // If we don't have the required values being passed in, just leave the
  // sample empty.
  StringBuilder sample_prefix;
  if (interface_name && strcmp("eval", interface_name) == 0) {
    sample_prefix.Append("eval");
  } else if (interface_name && property_name) {
    sample_prefix.Append(interface_name);
    sample_prefix.Append(" ");
    sample_prefix.Append(property_name);
  }
  return sample_prefix.ToString();
}

const char* GetElementName(const ScriptElementBase::Type type) {
  switch (type) {
    case ScriptElementBase::Type::kHTMLScriptElement:
      return "HTMLScriptElement";
    case ScriptElementBase::Type::kSVGScriptElement:
      return "SVGScriptElement";
  }
  NOTREACHED();
  return "";
}

HeapVector<ScriptValue> GetDefaultCallbackArgs(
    v8::Isolate* isolate,
    const char* type,
    const ExceptionState& exception_state) {
  ScriptState* script_state = ScriptState::Current(isolate);
  HeapVector<ScriptValue> args;
  args.push_back(ScriptValue::From(script_state, type));
  args.push_back(
      ScriptValue::From(script_state, GetSamplePrefix(exception_state)));
  return args;
}

// Handle failure of a Trusted Type assignment.
//
// If trusted type assignment fails, we need to
// - report the violation via CSP
// - increment the appropriate counter,
// - raise a JavaScript exception (if enforced).
//
// Returns whether the failure should be enforced.
bool TrustedTypeFail(TrustedTypeViolationKind kind,
                     const ExecutionContext* execution_context,
                     ExceptionState& exception_state,
                     const String& value) {
  if (!execution_context)
    return true;

  // Test case docs (MakeGarbageCollected<Document>()) might not have a window
  // and hence no TrustedTypesPolicyFactory.
  if (execution_context->GetTrustedTypes())
    execution_context->GetTrustedTypes()->CountTrustedTypeAssignmentError();

  bool allow =
      execution_context->GetSecurityContext()
          .GetContentSecurityPolicy()
          ->AllowTrustedTypeAssignmentFailure(GetMessage(kind), value,
                                              GetSamplePrefix(exception_state));
  if (!allow) {
    exception_state.ThrowTypeError(GetMessage(kind));
  }
  return !allow;
}

TrustedTypePolicy* GetDefaultPolicy(const ExecutionContext* execution_context) {
  DCHECK(execution_context);
  return execution_context->GetTrustedTypes()
             ? execution_context->GetTrustedTypes()->defaultPolicy()
             : nullptr;
}

// Functionally identical to TrustedTypesCheckForScript(const String&, ..), but
// to be called outside of regular script execution. This is required for both
// GetStringForScriptExecution & TrustedTypesCheckForJavascriptURLinNavigation,
// and has a number of additional parameters to enable proper error reporting
// for each case.
String GetStringFromScriptHelper(
    const String& script,
    Document* doc,

    // Parameters to customize error messages:
    const char* element_name_for_exception,
    const char* attribute_name_for_exception,
    TrustedTypeViolationKind violation_kind,
    TrustedTypeViolationKind violation_kind_when_default_policy_failed) {
  if (!doc)
    return script;
  bool require_trusted_type =
      RequireTrustedTypesCheck(doc->ToExecutionContext());
  if (!require_trusted_type)
    return script;

  // Set up JS context & friends.
  //
  // All other functions in here are expected to be called during JS execution,
  // where naturally everything is properly set up for more JS execution.
  // This one is called during navigation, and thus needs to do a bit more
  // work. We need two JavaScript-ish things:
  // - TrustedTypeFail expects an ExceptionState, which it will use to throw
  //   an exception. In our case, we will always clear the exception (as there
  //   is no user script to pass it to), and we only use this as a signalling
  //   mechanism.
  // - If the default policy applies, we need to execute the JS callback.
  //   Unlike the various ScriptController::Execute* and ..::Eval* methods,
  //   we are not executing a source String, but an already compiled callback
  //   function.
  v8::HandleScope handle_scope(doc->GetIsolate());
  ScriptState::Scope script_state_scope(
      ScriptState::From(static_cast<LocalWindowProxyManager*>(
                            doc->GetFrame()->GetWindowProxyManager())
                            ->MainWorldProxy()
                            ->ContextIfInitialized()));
  ExceptionState exception_state(
      doc->GetIsolate(), ExceptionState::kUnknownContext,
      element_name_for_exception, attribute_name_for_exception);

  TrustedTypePolicy* default_policy =
      GetDefaultPolicy(doc->ToExecutionContext());
  if (!default_policy) {
    if (TrustedTypeFail(violation_kind, doc->ToExecutionContext(),
                        exception_state, script)) {
      exception_state.ClearException();
      return String();
    }
    return script;
  }

  TrustedScript* result = default_policy->CreateScript(
      doc->GetIsolate(), script,
      GetDefaultCallbackArgs(doc->GetIsolate(), "TrustedScript",
                             exception_state),
      exception_state);
  if (exception_state.HadException()) {
    exception_state.ClearException();
    return String();
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(violation_kind_when_default_policy_failed,
                        doc->ToExecutionContext(), exception_state, script)) {
      exception_state.ClearException();
      return String();
    }
    return script;
  }
  return result->toString();
}

}  // namespace

bool RequireTrustedTypesCheck(const ExecutionContext* execution_context) {
  return execution_context && execution_context->RequireTrustedTypes() &&
         !ContentSecurityPolicy::ShouldBypassMainWorld(execution_context);
}

String TrustedTypesCheckForHTML(const String& html,
                                const ExecutionContext* execution_context,
                                ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return html;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedHTMLAssignment, execution_context,
                        exception_state, html)) {
      return g_empty_string;
    }
    return html;
  }

  if (!default_policy->HasCreateHTML()) {
    if (TrustedTypeFail(kTrustedHTMLAssignmentAndNoDefaultPolicyExisted,
                        execution_context, exception_state, html)) {
      return g_empty_string;
    } else {
      return html;
    }
  }
  TrustedHTML* result = default_policy->CreateHTML(
      execution_context->GetIsolate(), html,
      GetDefaultCallbackArgs(execution_context->GetIsolate(), "TrustedHTML",
                             exception_state),
      exception_state);
  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedHTMLAssignmentAndDefaultPolicyFailed,
                        execution_context, exception_state, html)) {
      return g_empty_string;
    } else {
      return html;
    }
  }

  return result->toString();
}

String TrustedTypesCheckForHTML(const String& html,
                                const Document* document,
                                ExceptionState& exception_state) {
  return TrustedTypesCheckForHTML(
      html, document ? document->GetExecutionContext() : nullptr,
      exception_state);
}

String TrustedTypesCheckForScript(const String& script,
                                  const ExecutionContext* execution_context,
                                  ExceptionState& exception_state) {
  bool require_trusted_type = RequireTrustedTypesCheck(execution_context);
  if (!require_trusted_type) {
    return script;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptAssignment, execution_context,
                        exception_state, script)) {
      return g_empty_string;
    }
    return script;
  }

  if (!default_policy->HasCreateScript()) {
    if (TrustedTypeFail(kTrustedScriptAssignmentAndNoDefaultPolicyExisted,
                        execution_context, exception_state, script)) {
      return g_empty_string;
    } else {
      return script;
    }
  }
  TrustedScript* result = default_policy->CreateScript(
      execution_context->GetIsolate(), script,
      GetDefaultCallbackArgs(execution_context->GetIsolate(), "TrustedScript",
                             exception_state),
      exception_state);
  DCHECK_EQ(!result, exception_state.HadException());
  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedScriptAssignmentAndDefaultPolicyFailed,
                        execution_context, exception_state, script)) {
      return g_empty_string;
    } else {
      return script;
    }
  }

  return result->toString();
}

String TrustedTypesCheckForScriptURL(const String& script_url,
                                     const ExecutionContext* execution_context,
                                     ExceptionState& exception_state) {
  bool require_trusted_type =
      RequireTrustedTypesCheck(execution_context) &&
      RuntimeEnabledFeatures::TrustedDOMTypesEnabled(execution_context);
  if (!require_trusted_type) {
    return script_url;
  }

  TrustedTypePolicy* default_policy = GetDefaultPolicy(execution_context);
  if (!default_policy) {
    if (TrustedTypeFail(kTrustedScriptURLAssignment, execution_context,
                        exception_state, script_url)) {
      return g_empty_string;
    }
    return script_url;
  }

  if (!default_policy->HasCreateScriptURL()) {
    if (TrustedTypeFail(kTrustedScriptURLAssignmentAndNoDefaultPolicyExisted,
                        execution_context, exception_state, script_url)) {
      return g_empty_string;
    } else {
      return script_url;
    }
  }
  TrustedScriptURL* result = default_policy->CreateScriptURL(
      execution_context->GetIsolate(), script_url,
      GetDefaultCallbackArgs(execution_context->GetIsolate(),
                             "TrustedScriptURL", exception_state),
      exception_state);

  if (exception_state.HadException()) {
    return g_empty_string;
  }

  if (result->toString().IsNull()) {
    if (TrustedTypeFail(kTrustedScriptURLAssignmentAndDefaultPolicyFailed,
                        execution_context, exception_state, script_url)) {
      return g_empty_string;
    } else {
      return script_url;
    }
  }

  return result->toString();
}

String TrustedTypesCheckFor(
    SpecificTrustedType type,
    const StringOrTrustedHTMLOrTrustedScriptOrTrustedScriptURL& trusted,
    const ExecutionContext* execution_context,
    ExceptionState& exception_state) {
  // Whatever happens below, we will need the string value:
  String value;
  if (trusted.IsTrustedHTML()) {
    value = trusted.GetAsTrustedHTML()->toString();
  } else if (trusted.IsTrustedScript()) {
    value = trusted.GetAsTrustedScript()->toString();
  } else if (trusted.IsTrustedScriptURL()) {
    value = trusted.GetAsTrustedScriptURL()->toString();
  } else if (trusted.IsString()) {
    value = trusted.GetAsString();
  }  // else: trusted.IsNull(). But we don't have anything to do in that case.

  // The check passes if we have the proper trusted type:
  if (type == SpecificTrustedType::kNone ||
      (trusted.IsTrustedHTML() && type == SpecificTrustedType::kHTML) ||
      (trusted.IsTrustedScript() && type == SpecificTrustedType::kScript) ||
      (trusted.IsTrustedScriptURL() &&
       type == SpecificTrustedType::kScriptURL)) {
    return value;
  }

  // In all other cases: run the full check against the string value.
  return TrustedTypesCheckFor(type, value, execution_context, exception_state);
}

String TrustedTypesCheckForScript(StringOrTrustedScript trusted,
                                  const ExecutionContext* execution_context,
                                  ExceptionState& exception_state) {
  // To remain compatible with legacy behaviour, HTMLElement uses extended IDL
  // attributes to allow for nullable union of (DOMString or TrustedScript).
  // Thus, this method is required to handle the case where
  // string_or_trusted_script.IsNull(), unlike the various similar methods in
  // this file.
  if (trusted.IsTrustedScript()) {
    return trusted.GetAsTrustedScript()->toString();
  }
  if (trusted.IsNull()) {
    trusted = StringOrTrustedScript::FromString(g_empty_string);
  }
  return TrustedTypesCheckForScript(trusted.GetAsString(), execution_context,
                                    exception_state);
}

String TrustedTypesCheckFor(SpecificTrustedType type,
                            const String& trusted,
                            const ExecutionContext* execution_context,
                            ExceptionState& exception_state) {
  switch (type) {
    case SpecificTrustedType::kHTML:
      return TrustedTypesCheckForHTML(trusted, execution_context,
                                      exception_state);
    case SpecificTrustedType::kScript:
      return TrustedTypesCheckForScript(trusted, execution_context,
                                        exception_state);
    case SpecificTrustedType::kScriptURL:
      return TrustedTypesCheckForScriptURL(trusted, execution_context,
                                           exception_state);
    case SpecificTrustedType::kNone:
      return trusted;
  }
  NOTREACHED();
  return "";
}

String CORE_EXPORT
GetStringForScriptExecution(const String& script,
                            const ScriptElementBase::Type type,
                            Document* doc) {
  return GetStringFromScriptHelper(script, doc, GetElementName(type), "text",
                                   kScriptExecution,
                                   kScriptExecutionAndDefaultPolicyFailed);
}

String TrustedTypesCheckForJavascriptURLinNavigation(
    const String& javascript_url,
    Document* doc) {
  return GetStringFromScriptHelper(
      javascript_url, doc, "Location", "href", kNavigateToJavascriptURL,
      kNavigateToJavascriptURLAndDefaultPolicyFailed);
}

}  // namespace blink
