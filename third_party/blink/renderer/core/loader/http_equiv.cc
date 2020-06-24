// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/http_equiv.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/private/frame_client_hints_preferences_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/client_hints_preferences.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

// Returns true if execution of scripts from the url are allowed. Compared to
// AllowScriptFromSource(), this method does not generate any
// notification to the |ContentSettingsClient| that the execution of the
// script was blocked. This method should be called only when there is a need
// to check the settings, and where blocked setting doesn't really imply that
// JavaScript was blocked from being executed.
bool AllowScriptFromSourceWithoutNotifying(
    const KURL& url,
    WebContentSettingsClient* settings_client,
    Settings* settings) {
  bool allow_script = !settings || settings->GetScriptEnabled();
  if (settings_client)
    allow_script = settings_client->AllowScriptFromSource(allow_script, url);
  return allow_script;
}

// Gets the url of the currently executing script. Returns empty url, if no
// script is executing (e.g. during parsing of a meta tag in markup), or the
// script context is otherwise unavailable.
// TODO(crbug.com/1073920): Extract this function into a reusable location. This
// function was cloned from:
// https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/core/frame/ad_tracker.cc;l=92;drc=51291f88a8f94602d26403716e2dfa781f8846ee?originalUrl=https:%2F%2Fcs.chromium.org%2F
// There's also a similar implementation here:
// https://source.chromium.org/chromium/chromium/src/+/master:third_party/blink/renderer/core/inspector/inspector_dom_snapshot_agent.cc;l=97;drc=f002f3b90c510002b98aa08c2fe7f3d93372007e?originalUrl=https:%2F%2Fcs.chromium.org%2F
KURL GetCurrentScriptUrl() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate || !isolate->InContext())
    return NullURL();

  // CurrentStackTrace is 10x faster than CaptureStackTrace if all that you need
  // is the url of the script at the top of the stack. See crbug.com/1057211 for
  // more detail.
  v8::Local<v8::StackTrace> stack_trace =
      v8::StackTrace::CurrentStackTrace(isolate, /*frame_limit=*/1);
  if (stack_trace.IsEmpty() || stack_trace->GetFrameCount() < 1)
    return NullURL();

  v8::Local<v8::StackFrame> frame = stack_trace->GetFrame(isolate, 0);
  v8::Local<v8::String> script_name = frame->GetScriptNameOrSourceURL();
  if (script_name.IsEmpty() || !script_name->Length())
    return NullURL();

  return KURL(ToCoreString(script_name));
}

}  // namespace

void HttpEquiv::Process(Document& document,
                        const AtomicString& equiv,
                        const AtomicString& content,
                        bool in_document_head_element,
                        Element* element) {
  DCHECK(!equiv.IsNull());
  DCHECK(!content.IsNull());

  if (EqualIgnoringASCIICase(equiv, "default-style")) {
    ProcessHttpEquivDefaultStyle(document, content);
  } else if (EqualIgnoringASCIICase(equiv, "refresh")) {
    ProcessHttpEquivRefresh(document, content, element);
  } else if (EqualIgnoringASCIICase(equiv, "set-cookie")) {
    ProcessHttpEquivSetCookie(document, content, element);
  } else if (EqualIgnoringASCIICase(equiv, "content-language")) {
    document.SetContentLanguage(content);
  } else if (EqualIgnoringASCIICase(equiv, "x-dns-prefetch-control")) {
    document.ParseDNSPrefetchControlHeader(content);
  } else if (EqualIgnoringASCIICase(equiv, "x-frame-options")) {
    document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kSecurity,
        mojom::ConsoleMessageLevel::kError,
        "X-Frame-Options may only be set via an HTTP header sent along with a "
        "document. It may not be set inside <meta>."));
  } else if (EqualIgnoringASCIICase(equiv, http_names::kAcceptCH)) {
    ProcessHttpEquivAcceptCH(document, content);
  } else if (EqualIgnoringASCIICase(equiv, "content-security-policy") ||
             EqualIgnoringASCIICase(equiv,
                                    "content-security-policy-report-only")) {
    if (in_document_head_element)
      ProcessHttpEquivContentSecurityPolicy(document, equiv, content);
    else
      document.GetContentSecurityPolicy()->ReportMetaOutsideHead(content);
  } else if (EqualIgnoringASCIICase(equiv, http_names::kOriginTrial)) {
    if (in_document_head_element) {
      ProcessHttpEquivOriginTrial(document, content);
    }
  }
}

void HttpEquiv::ProcessHttpEquivContentSecurityPolicy(
    Document& document,
    const AtomicString& equiv,
    const AtomicString& content) {
  if (document.ImportLoader())
    return;
  if (document.GetSettings() && document.GetSettings()->BypassCSP())
    return;
  if (EqualIgnoringASCIICase(equiv, "content-security-policy")) {
    document.GetContentSecurityPolicy()->DidReceiveHeader(
        content, network::mojom::ContentSecurityPolicyType::kEnforce,
        network::mojom::ContentSecurityPolicySource::kMeta);
  } else if (EqualIgnoringASCIICase(equiv,
                                    "content-security-policy-report-only")) {
    document.GetContentSecurityPolicy()->DidReceiveHeader(
        content, network::mojom::ContentSecurityPolicyType::kReport,
        network::mojom::ContentSecurityPolicySource::kMeta);
  } else {
    NOTREACHED();
  }
}

void HttpEquiv::ProcessHttpEquivAcceptCH(Document& document,
                                         const AtomicString& content) {
  LocalFrame* frame = document.GetFrame();
  if (!frame)
    return;

  if (!document.GetFrame()->IsMainFrame()) {
    return;
  }

  if (!AllowScriptFromSourceWithoutNotifying(
          document.Url(), document.GetFrame()->GetContentSettingsClient(),
          document.GetFrame()->GetSettings())) {
    // Do not allow configuring client hints if JavaScript is disabled.
    return;
  }

  UseCounter::Count(document, WebFeature::kClientHintsMetaAcceptCH);
  FrameClientHintsPreferencesContext hints_context(frame);
  frame->GetClientHintsPreferences().UpdateFromAcceptClientHintsHeader(
      content, document.Url(), ClientHintsPreferences::UpdateMode::kMerge,
      &hints_context);
}

void HttpEquiv::ProcessHttpEquivDefaultStyle(Document& document,
                                             const AtomicString& content) {
  document.GetStyleEngine().SetHttpDefaultStyle(content);
}

void HttpEquiv::ProcessHttpEquivOriginTrial(Document& document,
                                            const AtomicString& content) {
  // For meta tags injected by script, process the token with the origin of the
  // external script, if available.
  // NOTE: The external script origin is not considered security-critical. See
  // the comment thread in the design doc for details:
  // https://docs.google.com/document/d/1xALH9W7rWmX0FpjudhDeS2TNTEOXuPn4Tlc9VmuPdHA/edit?disco=AAAAJyG8StI
  if (RuntimeEnabledFeatures::ThirdPartyOriginTrialsEnabled()) {
    KURL external_script_url = GetCurrentScriptUrl();

    if (external_script_url.IsValid()) {
      scoped_refptr<SecurityOrigin> external_origin =
          SecurityOrigin::Create(external_script_url);
      document.GetOriginTrialContext()->AddTokenFromExternalScript(
          content, external_origin.get());
      return;
    }
  }

  // Process token as usual, without an external script origin.
  document.GetOriginTrialContext()->AddToken(content);
}

void HttpEquiv::ProcessHttpEquivRefresh(Document& document,
                                        const AtomicString& content,
                                        Element* element) {
  UseCounter::Count(document, WebFeature::kMetaRefresh);
  if (!document.GetContentSecurityPolicy()->AllowInline(
          ContentSecurityPolicy::InlineType::kScript, element, "" /* content */,
          "" /* nonce */, NullURL(), OrdinalNumber(),
          ReportingDisposition::kSuppressReporting)) {
    UseCounter::Count(document,
                      WebFeature::kMetaRefreshWhenCSPBlocksInlineScript);
  }

  document.MaybeHandleHttpRefresh(content, Document::kHttpRefreshFromMetaTag);
}

void HttpEquiv::ProcessHttpEquivSetCookie(Document& document,
                                          const AtomicString& content,
                                          Element* element) {
  document.AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kSecurity,
      mojom::ConsoleMessageLevel::kError,
      String::Format("Blocked setting the `%s` cookie from a `<meta>` tag.",
                     content.Utf8().c_str())));
}

}  // namespace blink
