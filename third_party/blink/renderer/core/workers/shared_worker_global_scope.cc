/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"

#include <memory>
#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

// static
SharedWorkerGlobalScope* SharedWorkerGlobalScope::Create(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    SharedWorkerThread* thread,
    base::TimeTicks time_origin) {
  // Off-the-main-thread worker script fetch:
  // Initialize() is called after script fetch.
  if (creation_params->off_main_thread_fetch_option ==
      OffMainThreadWorkerScriptFetchOption::kEnabled) {
    return MakeGarbageCollected<SharedWorkerGlobalScope>(
        std::move(creation_params), thread, time_origin);
  }

  // Legacy on-the-main-thread worker script fetch (to be removed):
  KURL response_script_url = creation_params->script_url;
  network::mojom::ReferrerPolicy response_referrer_policy =
      creation_params->referrer_policy;
  mojom::IPAddressSpace response_address_space =
      *creation_params->response_address_space;
  // Contrary to the name, |outside_content_security_policy_headers| contains
  // worker script's response CSP headers in this case.
  // TODO(nhiroki): Introduce inside's csp headers field in
  // GlobalScopeCreationParams or deprecate this code path by enabling
  // off-the-main-thread worker script fetch by default.
  Vector<CSPHeaderAndType> response_csp_headers =
      creation_params->outside_content_security_policy_headers;
  std::unique_ptr<Vector<String>> response_origin_trial_tokens =
      std::move(creation_params->origin_trial_tokens);
  auto* global_scope = MakeGarbageCollected<SharedWorkerGlobalScope>(
      std::move(creation_params), thread, time_origin);
  global_scope->Initialize(response_script_url, response_referrer_policy,
                           response_address_space, response_csp_headers,
                           response_origin_trial_tokens.get());
  return global_scope;
}

SharedWorkerGlobalScope::SharedWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    SharedWorkerThread* thread,
    base::TimeTicks time_origin)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin) {
  // When off-the-main-thread script fetch is enabled, ReadyToRunWorkerScript()
  // will be called after an app cache is selected.
  if (!features::IsOffMainThreadSharedWorkerScriptFetchEnabled())
    ReadyToRunWorkerScript();
}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope() = default;

const AtomicString& SharedWorkerGlobalScope::InterfaceName() const {
  return event_target_names::kSharedWorkerGlobalScope;
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::Initialize(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    mojom::IPAddressSpace response_address_space,
    const Vector<CSPHeaderAndType>& response_csp_headers,
    const Vector<String>* response_origin_trial_tokens) {
  // Step 12.3. "Set worker global scope's url to response's url."
  InitializeURL(response_url);

  // Step 12.4. "Set worker global scope's HTTPS state to response's HTTPS
  // state."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 12.5. "Set worker global scope's referrer policy to the result of
  // parsing the `Referrer-Policy` header of response."
  SetReferrerPolicy(response_referrer_policy);

  // https://wicg.github.io/cors-rfc1918/#integration-html
  SetAddressSpace(response_address_space);

  // Step 12.6. "Execute the Initialize a global object's CSP list algorithm
  // on worker global scope and response. [CSP]"
  // These should be called after SetAddressSpace() to correctly override the
  // address space by the "treat-as-public-address" CSP directive.
  InitContentSecurityPolicyFromVector(response_csp_headers);
  BindContentSecurityPolicyToExecutionContext();

  OriginTrialContext::AddTokens(this, response_origin_trial_tokens);

  // This should be called after OriginTrialContext::AddTokens() to install
  // origin trial features in JavaScript's global object.
  ScriptController()->PrepareForEvaluation();
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::FetchAndRunClassicScript(
    const KURL& script_url,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
  DCHECK(!IsContextPaused());

  // Step 12. "Fetch a classic worker script given url, outside settings,
  // destination, and inside settings."
  auto destination = mojom::RequestContextType::SHARED_WORKER;

  // Step 12.1. "Set request's reserved client to inside settings."
  // The browesr process takes care of this.

  // Step 12.2. "Fetch request, and asynchronously wait to run the remaining
  // steps as part of fetch's process response for the response response."
  WorkerClassicScriptLoader* classic_script_loader =
      MakeGarbageCollected<WorkerClassicScriptLoader>();
  classic_script_loader->LoadTopLevelScriptAsynchronously(
      *this,
      CreateOutsideSettingsFetcher(outside_settings_object,
                                   outside_resource_timing_notifier),
      script_url, destination, network::mojom::RequestMode::kSameOrigin,
      network::mojom::CredentialsMode::kSameOrigin,
      WTF::Bind(&SharedWorkerGlobalScope::DidReceiveResponseForClassicScript,
                WrapWeakPersistent(this),
                WrapPersistent(classic_script_loader)),
      WTF::Bind(&SharedWorkerGlobalScope::DidFetchClassicScript,
                WrapWeakPersistent(this), WrapPersistent(classic_script_loader),
                stack_id));
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::FetchAndRunModuleScript(
    const KURL& module_url_record,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode) {
  // Step 12: "Let destination be "sharedworker" if is shared is true, and
  // "worker" otherwise."

  // Step 13: "... Fetch a module worker script graph given url, outside
  // settings, destination, the value of the credentials member of options, and
  // inside settings."

  // TODO(nhiroki): Implement module loading for shared workers.
  // (https://crbug.com/824646)
  NOTREACHED();
}

const String SharedWorkerGlobalScope::name() const {
  return Name();
}

void SharedWorkerGlobalScope::Connect(MessagePortChannel channel) {
  DCHECK(!IsContextPaused());
  auto* port = MakeGarbageCollected<MessagePort>(*this);
  port->Entangle(std::move(channel));
  MessageEvent* event =
      MessageEvent::Create(MakeGarbageCollected<MessagePortArray>(1, port),
                           String(), String(), port);
  event->initEvent(event_type_names::kConnect, false, false);
  DispatchEvent(*event);
}

void SharedWorkerGlobalScope::OnAppCacheSelected() {
  DCHECK(IsContextThread());
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
  ReadyToRunWorkerScript();
}

void SharedWorkerGlobalScope::DidReceiveResponseForClassicScript(
    WorkerClassicScriptLoader* classic_script_loader) {
  DCHECK(IsContextThread());
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
  probe::DidReceiveScriptResponse(this, classic_script_loader->Identifier());
}

// https://html.spec.whatwg.org/C/#worker-processing-model
void SharedWorkerGlobalScope::DidFetchClassicScript(
    WorkerClassicScriptLoader* classic_script_loader,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsContextThread());
  DCHECK(features::IsOffMainThreadSharedWorkerScriptFetchEnabled());

  // Step 12. "If the algorithm asynchronously completes with null, then:"
  if (classic_script_loader->Failed()) {
    // Step 12.1. "Queue a task to fire an event named error at worker."
    // Step 12.2. "Run the environment discarding steps for inside settings."
    // Step 12.3. "Return."
    ReportingProxy().DidFailToFetchClassicScript();
    return;
  }
  ReportingProxy().DidFetchScript(classic_script_loader->AppCacheID());
  probe::ScriptImported(this, classic_script_loader->Identifier(),
                        classic_script_loader->SourceText());

  auto response_referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  if (!classic_script_loader->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        classic_script_loader->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &response_referrer_policy);
  }

  // Step 12.3-12.6 are implemented in Initialize().
  Initialize(classic_script_loader->ResponseURL(), response_referrer_policy,
             classic_script_loader->ResponseAddressSpace(),
             classic_script_loader->GetContentSecurityPolicy()
                 ? classic_script_loader->GetContentSecurityPolicy()->Headers()
                 : Vector<CSPHeaderAndType>(),
             classic_script_loader->OriginTrialTokens());

  // Step 12.7. "Asynchronously complete the perform the fetch steps with
  // response."
  EvaluateClassicScript(
      classic_script_loader->ResponseURL(), classic_script_loader->SourceText(),
      classic_script_loader->ReleaseCachedMetadata(), stack_id);
}

void SharedWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

void SharedWorkerGlobalScope::Trace(blink::Visitor* visitor) {
  WorkerGlobalScope::Trace(visitor);
}

}  // namespace blink
