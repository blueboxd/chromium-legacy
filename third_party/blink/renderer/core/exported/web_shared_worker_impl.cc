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

#include "third_party/blink/renderer/core/exported/web_shared_worker_impl.h"

#include <memory>
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-blink.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/frame_loader.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/core/workers/shared_worker_content_settings_proxy.h"
#include "third_party/blink/renderer/core/workers/shared_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/shared_worker_thread.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WebSharedWorkerImpl::WebSharedWorkerImpl(
    WebSharedWorkerClient* client,
    const base::UnguessableToken& appcache_host_id)
    : client_(client),
      creation_address_space_(mojom::IPAddressSpace::kPublic),
      parent_execution_context_task_runners_(
          ParentExecutionContextTaskRunners::Create()),
      appcache_host_id_(appcache_host_id) {
  DCHECK(IsMainThread());
}

WebSharedWorkerImpl::~WebSharedWorkerImpl() {
  DCHECK(IsMainThread());
}

void WebSharedWorkerImpl::TerminateWorkerThread() {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;
  if (shadow_page_ && !shadow_page_->WasInitialized()) {
    client_->WorkerScriptLoadFailed();
    // The worker thread hasn't been started yet. Immediately notify the client
    // of worker termination.
    client_->WorkerContextDestroyed();
    // |this| is deleted at this point.
    return;
  }
  if (main_script_loader_) {
    main_script_loader_->Cancel();
    main_script_loader_ = nullptr;
    client_->WorkerScriptLoadFailed();
    // The worker thread hasn't been started yet. Immediately notify the client
    // of worker termination.
    client_->WorkerContextDestroyed();
    // |this| is deleted at this point.
    return;
  }
  if (worker_thread_) {
    worker_thread_->Terminate();
    DevToolsAgent::WorkerThreadTerminated(shadow_page_->GetDocument(),
                                          worker_thread_.get());
  }
}

void WebSharedWorkerImpl::OnShadowPageInitialized() {
  DCHECK(IsMainThread());
  DCHECK(!main_script_loader_);

  // This shadow page's address space will be used for creating outside
  // FetchClientSettingsObject.
  shadow_page_->GetDocument()->SetAddressSpace(creation_address_space_);

  shadow_page_->DocumentLoader()->SetServiceWorkerNetworkProvider(
      client_->CreateServiceWorkerNetworkProvider());

  if (features::IsOffMainThreadSharedWorkerScriptFetchEnabled()) {
    // Bypass main script loading on the main thread.
    ContinueStartWorkerContext();
    return;
  }

  main_script_loader_ = MakeGarbageCollected<WorkerClassicScriptLoader>();
  main_script_loader_->LoadTopLevelScriptAsynchronously(
      *shadow_page_->GetDocument(), shadow_page_->GetDocument()->Fetcher(),
      script_request_url_, mojom::RequestContextType::SHARED_WORKER,
      network::mojom::RequestMode::kSameOrigin,
      network::mojom::CredentialsMode::kSameOrigin,
      Bind(&WebSharedWorkerImpl::DidReceiveScriptLoaderResponse,
           WTF::Unretained(this)),
      Bind(&WebSharedWorkerImpl::OnScriptLoaderFinished,
           WTF::Unretained(this)));
  // Do nothing here since OnScriptLoaderFinished() might have been already
  // invoked and |this| might have been deleted at this point.
}

void WebSharedWorkerImpl::ResumeStartup() {
  DCHECK(IsMainThread());
  bool is_paused_on_start = is_paused_on_start_;
  is_paused_on_start_ = false;
  if (is_paused_on_start) {
    // We'll continue in OnShadowPageInitialized().
    shadow_page_->Initialize(script_request_url_);
  }
}

const base::UnguessableToken& WebSharedWorkerImpl::GetDevToolsWorkerToken() {
  return devtools_worker_token_;
}

void WebSharedWorkerImpl::CountFeature(WebFeature feature) {
  DCHECK(IsMainThread());
  client_->CountFeature(feature);
}

void WebSharedWorkerImpl::DidFetchScript(int64_t app_cache_id) {
  DCHECK(IsMainThread());
  Document* document = shadow_page_->GetDocument();
  ApplicationCacheHost* host = document->Loader()->GetApplicationCacheHost();
  if (host) {
    host->SelectCacheForSharedWorker(
        app_cache_id, WTF::Bind(&WebSharedWorkerImpl::OnAppCacheSelected,
                                weak_ptr_factory_.GetWeakPtr()));
  } else {
    OnAppCacheSelected();
  }
}

void WebSharedWorkerImpl::DidFailToFetchClassicScript() {
  DCHECK(IsMainThread());
  client_->WorkerScriptLoadFailed();
  TerminateWorkerThread();
  // DidTerminateWorkerThread() will be called asynchronously.
}

void WebSharedWorkerImpl::DidEvaluateClassicScript(bool success) {
  DCHECK(IsMainThread());
  client_->WorkerScriptEvaluated(success);
}

void WebSharedWorkerImpl::DidCloseWorkerGlobalScope() {
  DCHECK(IsMainThread());
  client_->WorkerContextClosed();
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::DidTerminateWorkerThread() {
  DCHECK(IsMainThread());
  client_->WorkerContextDestroyed();
  // |this| is deleted at this point.
}

void WebSharedWorkerImpl::Connect(MessagePortChannel web_channel) {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;
  // The HTML spec requires to queue a connect event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/C/#shared-workers-and-the-sharedworker-interface
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(&WebSharedWorkerImpl::ConnectTaskOnWorkerThread,
                          WTF::CrossThreadUnretained(this),
                          WTF::Passed(std::move(web_channel))));
}

void WebSharedWorkerImpl::ConnectTaskOnWorkerThread(
    MessagePortChannel channel) {
  // Wrap the passed-in channel in a MessagePort, and send it off via a connect
  // event.
  DCHECK(worker_thread_->IsCurrentThread());
  auto* scope = To<SharedWorkerGlobalScope>(worker_thread_->GlobalScope());
  scope->Connect(std::move(channel));
}

void WebSharedWorkerImpl::StartWorkerContext(
    const WebURL& script_request_url,
    const WebString& name,
    const WebString& content_security_policy,
    mojom::ContentSecurityPolicyType policy_type,
    mojom::IPAddressSpace creation_address_space,
    const base::UnguessableToken& devtools_worker_token,
    PrivacyPreferences privacy_preferences,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    mojo::ScopedMessagePipeHandle content_settings_handle,
    mojo::ScopedMessagePipeHandle interface_provider) {
  DCHECK(IsMainThread());
  script_request_url_ = script_request_url;
  name_ = name;
  creation_address_space_ = creation_address_space;
  // Chrome doesn't use interface versioning.
  content_settings_info_ = mojom::blink::WorkerContentSettingsProxyPtrInfo(
      std::move(content_settings_handle), 0u);
  pending_interface_provider_.set_handle(std::move(interface_provider));

  devtools_worker_token_ = devtools_worker_token;
  // |shadow_page_| must be created after |devtools_worker_token_| because it
  // triggers creation of a InspectorNetworkAgent that tries to access the
  // token.
  shadow_page_ = std::make_unique<WorkerShadowPage>(
      this, std::move(loader_factory), std::move(privacy_preferences),
      appcache_host_id_);

  // If we were asked to pause worker context on start and wait for debugger
  // then now is a good time to do that.
  client_->WorkerReadyForInspection();
  if (pause_worker_context_on_start_) {
    is_paused_on_start_ = true;
    return;
  }

  // We'll continue in OnShadowPageInitialized().
  shadow_page_->Initialize(script_request_url_);
}

void WebSharedWorkerImpl::DidReceiveScriptLoaderResponse() {
  DCHECK(IsMainThread());
  probe::DidReceiveScriptResponse(shadow_page_->GetDocument(),
                                  main_script_loader_->Identifier());
}

void WebSharedWorkerImpl::OnScriptLoaderFinished() {
  DCHECK(IsMainThread());
  DCHECK(main_script_loader_);
  if (asked_to_terminate_)
    return;
  if (main_script_loader_->Failed()) {
    main_script_loader_->Cancel();
    main_script_loader_ = nullptr;
    client_->WorkerScriptLoadFailed();
    // The worker thread hasn't been started yet. Immediately notify the client
    // of worker termination.
    client_->WorkerContextDestroyed();
    // |this| is deleted at this point.
    return;
  }
  DidFetchScript(main_script_loader_->AppCacheID());
  probe::ScriptImported(shadow_page_->GetDocument(),
                        main_script_loader_->Identifier(),
                        main_script_loader_->SourceText());

  // The browser process is expected to send a SetController IPC before sending
  // the script response, but there is no guarantee of the ordering as the
  // messages arrive on different message pipes. Wait for the SetController IPC
  // to be received before starting the worker; otherwise fetches from the
  // worker might not go through the appropriate controller.
  client_->WaitForServiceWorkerControllerInfo(
      shadow_page_->DocumentLoader()->GetServiceWorkerNetworkProvider(),
      WTF::Bind(&WebSharedWorkerImpl::ContinueStartWorkerContext,
                weak_ptr_factory_.GetWeakPtr()));
}

void WebSharedWorkerImpl::OnAppCacheSelected() {
  DCHECK(IsMainThread());
  if (features::IsOffMainThreadSharedWorkerScriptFetchEnabled()) {
    DCHECK(GetWorkerThread());
    GetWorkerThread()->OnAppCacheSelected();
  }
}

void WebSharedWorkerImpl::ContinueStartWorkerContext() {
  DCHECK(IsMainThread());
  if (asked_to_terminate_)
    return;

  // FIXME: this document's origin is pristine and without any extra privileges
  // (e.g. GrantUniversalAccess) that can be overriden in regular documents
  // via WebPreference by embedders. (crbug.com/254993)
  Document* document = shadow_page_->GetDocument();

  // Creates 'outside settings' used in the "Processing model" algorithm in the
  // HTML spec:
  // https://html.spec.whatwg.org/C/#worker-processing-model
  //
  // TODO(nhiroki): According to the spec, the 'outside settings' should
  // correspond to the Document that called 'new SharedWorker()'. However,
  // for now there is no way to pass the settings object over mojo IPCs, so as
  // a stopgap the shadow page's Document is used here.
  auto* outside_settings_object =
      MakeGarbageCollected<FetchClientSettingsObjectSnapshot>(
          document->Fetcher()->GetProperties().GetFetchClientSettingsObject());

  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context =
      client_->CreateWorkerFetchContext(
          shadow_page_->DocumentLoader()->GetServiceWorkerNetworkProvider());
  DCHECK(web_worker_fetch_context);
  web_worker_fetch_context->SetApplicationCacheHostID(
      document->Loader()->GetApplicationCacheHost()->GetHostID());

  // TODO(nhiroki); Set |script_type| to mojom::ScriptType::kModule for module
  // fetch (https://crbug.com/824646).
  mojom::ScriptType script_type = mojom::ScriptType::kClassic;

  auto worker_settings = std::make_unique<WorkerSettings>(
      false /* disable_reading_from_canvas */,
      false /* strict_mixed_content_checking */,
      true /* allow_running_of_insecure_content */,
      false /* strictly_block_blockable_mixed_content */,
      GenericFontFamilySettings());

  if (features::IsOffMainThreadSharedWorkerScriptFetchEnabled()) {
    // Off-the-main-thread script fetch:
    // Some params (e.g., referrer policy, address space, CSP) passed to
    // GlobalScopeCreationParams are dummy values. They will be updated after
    // worker script fetch on the worker thread.
    // TODO(nhiroki): Currently |origin_trial_tokens| is not updated after
    // worker script fetch. Update them.
    auto creation_params = std::make_unique<GlobalScopeCreationParams>(
        script_request_url_, script_type,
        OffMainThreadWorkerScriptFetchOption::kEnabled, name_,
        document->UserAgent(), std::move(web_worker_fetch_context),
        Vector<CSPHeaderAndType>(), network::mojom::ReferrerPolicy::kDefault,
        outside_settings_object->GetSecurityOrigin(),
        document->IsSecureContext(), outside_settings_object->GetHttpsState(),
        CreateWorkerClients(), base::nullopt /* response_address_space */,
        nullptr /* origin_trial_tokens */, devtools_worker_token_,
        std::move(worker_settings), kV8CacheOptionsDefault,
        nullptr /* worklet_module_response_map */,
        std::move(pending_interface_provider_), BeginFrameProviderParams(),
        nullptr /* parent_feature_policy */, base::UnguessableToken());
    StartWorkerThread(std::move(creation_params), script_request_url_,
                      String() /* source_code */, *outside_settings_object);
    return;
  }

  // On-the-main-thread script fetch:
  DCHECK(main_script_loader_);
  DCHECK(!main_script_loader_->Failed());

  WebURL script_response_url = main_script_loader_->ResponseURL();
  DCHECK(script_request_url_ == script_response_url ||
         SecurityOrigin::AreSameSchemeHostPort(script_request_url_,
                                               script_response_url));
  auto referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  if (!main_script_loader_->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        main_script_loader_->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  }
  ContentSecurityPolicy* content_security_policy =
      main_script_loader_->GetContentSecurityPolicy();

  auto creation_params = std::make_unique<GlobalScopeCreationParams>(
      script_response_url, script_type,
      OffMainThreadWorkerScriptFetchOption::kDisabled, name_,
      document->UserAgent(), std::move(web_worker_fetch_context),
      content_security_policy ? content_security_policy->Headers()
                              : Vector<CSPHeaderAndType>(),
      referrer_policy, outside_settings_object->GetSecurityOrigin(),
      document->IsSecureContext(), outside_settings_object->GetHttpsState(),
      CreateWorkerClients(), main_script_loader_->ResponseAddressSpace(),
      main_script_loader_->OriginTrialTokens(), devtools_worker_token_,
      std::move(worker_settings), kV8CacheOptionsDefault,
      nullptr /* worklet_module_response_map */,
      std::move(pending_interface_provider_));
  StartWorkerThread(std::move(creation_params), script_response_url,
                    main_script_loader_->SourceText(),
                    *outside_settings_object);
  main_script_loader_ = nullptr;
}

void WebSharedWorkerImpl::StartWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const KURL& script_response_url,
    const String& source_code,
    const FetchClientSettingsObjectSnapshot& outside_settings_object) {
  DCHECK(IsMainThread());
  reporting_proxy_ = MakeGarbageCollected<SharedWorkerReportingProxy>(
      this, parent_execution_context_task_runners_);
  worker_thread_ = std::make_unique<SharedWorkerThread>(*reporting_proxy_);

  auto thread_startup_data = WorkerBackingThreadStartupData::CreateDefault();
  thread_startup_data.atomics_wait_mode =
      WorkerBackingThreadStartupData::AtomicsWaitMode::kAllow;
  auto devtools_params = DevToolsAgent::WorkerThreadCreated(
      shadow_page_->GetDocument(), GetWorkerThread(), script_response_url,
      name_);

  GetWorkerThread()->Start(std::move(global_scope_creation_params),
                           thread_startup_data, std::move(devtools_params),
                           parent_execution_context_task_runners_);
  // TODO(nhiroki): Support module workers (https://crbug.com/680046).
  if (features::IsOffMainThreadSharedWorkerScriptFetchEnabled()) {
    // The script has not yet been fetched. Fetch it now.

    // Currently we don't plumb performance timing for toplevel shared worker
    // script fetch. https://crbug.com/954005
    auto* resource_timing_notifier =
        MakeGarbageCollected<NullWorkerResourceTimingNotifier>();
    GetWorkerThread()->FetchAndRunClassicScript(
        script_request_url_, outside_settings_object, *resource_timing_notifier,
        v8_inspector::V8StackTraceId());
    // We continue in WorkerGlobalScope::EvaluateClassicScript() on the worker
    // thread.
  } else {
    // The script was already fetched on the main thread. Evaluate it now.
    GetWorkerThread()->EvaluateClassicScript(script_response_url, source_code,
                                             nullptr /* cached_meta_data */,
                                             v8_inspector::V8StackTraceId());
  }
}

WorkerClients* WebSharedWorkerImpl::CreateWorkerClients() {
  auto* worker_clients = MakeGarbageCollected<WorkerClients>();
  CoreInitializer::GetInstance().ProvideLocalFileSystemToWorker(
      *worker_clients);
  CoreInitializer::GetInstance().ProvideIndexedDBClientToWorker(
      *worker_clients);
  ProvideContentSettingsClientToWorker(
      worker_clients, std::make_unique<SharedWorkerContentSettingsProxy>(
                          std::move(content_settings_info_)));
  return worker_clients;
}

void WebSharedWorkerImpl::TerminateWorkerContext() {
  DCHECK(IsMainThread());
  TerminateWorkerThread();
}

void WebSharedWorkerImpl::PauseWorkerContextOnStart() {
  pause_worker_context_on_start_ = true;
}

void WebSharedWorkerImpl::BindDevToolsAgent(
    mojo::ScopedInterfaceEndpointHandle devtools_agent_host_ptr_info,
    mojo::ScopedInterfaceEndpointHandle devtools_agent_request) {
  shadow_page_->DevToolsAgent()->BindRequest(
      mojom::blink::DevToolsAgentHostAssociatedPtrInfo(
          std::move(devtools_agent_host_ptr_info),
          mojom::blink::DevToolsAgentHost::Version_),
      mojom::blink::DevToolsAgentAssociatedRequest(
          std::move(devtools_agent_request)));
}

scoped_refptr<base::SingleThreadTaskRunner> WebSharedWorkerImpl::GetTaskRunner(
    TaskType task_type) {
  return parent_execution_context_task_runners_->Get(task_type);
}

std::unique_ptr<WebSharedWorker> WebSharedWorker::Create(
    WebSharedWorkerClient* client,
    const base::UnguessableToken& appcache_host_id) {
  return base::WrapUnique(new WebSharedWorkerImpl(client, appcache_host_id));
}

}  // namespace blink
