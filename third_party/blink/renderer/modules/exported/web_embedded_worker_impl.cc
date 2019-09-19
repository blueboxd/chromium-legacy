/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>
#include <utility>
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/referrer_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/loader/frame_load_request.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread_startup_data.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// static
std::unique_ptr<WebEmbeddedWorker> WebEmbeddedWorker::Create(
    WebServiceWorkerContextClient* client,
    mojo::ScopedMessagePipeHandle cache_storage,
    mojo::ScopedMessagePipeHandle interface_provider,
    mojo::ScopedMessagePipeHandle browser_interface_broker) {
  return std::make_unique<WebEmbeddedWorkerImpl>(
      std::move(client),
      mojo::PendingRemote<mojom::blink::CacheStorage>(
          std::move(cache_storage), mojom::blink::CacheStorage::Version_),
      service_manager::mojom::blink::InterfaceProviderPtrInfo(
          std::move(interface_provider),
          service_manager::mojom::blink::InterfaceProvider::Version_),
      mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>(
          std::move(browser_interface_broker),
          mojom::blink::BrowserInterfaceBroker::Version_));
}

// static
std::unique_ptr<WebEmbeddedWorkerImpl> WebEmbeddedWorkerImpl::CreateForTesting(
    WebServiceWorkerContextClient* client) {
  auto worker_impl = std::make_unique<WebEmbeddedWorkerImpl>(
      client, mojo::NullRemote() /* cache_storage */,
      nullptr /* interface_provider_info */,
      mojo::NullRemote() /* browser_interface_broker */);
  return worker_impl;
}

WebEmbeddedWorkerImpl::WebEmbeddedWorkerImpl(
    WebServiceWorkerContextClient* client,
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote,
    service_manager::mojom::blink::InterfaceProviderPtrInfo
        interface_provider_info,
    mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
        browser_interface_broker)
    : worker_context_client_(client),
      cache_storage_remote_(std::move(cache_storage_remote)),
      interface_provider_info_(std::move(interface_provider_info)),
      browser_interface_broker_(std::move(browser_interface_broker)) {}

WebEmbeddedWorkerImpl::~WebEmbeddedWorkerImpl() {
  // TerminateWorkerContext() must be called before the destructor.
  DCHECK(asked_to_terminate_);
}

void WebEmbeddedWorkerImpl::StartWorkerContext(
    const WebEmbeddedWorkerStartData& data,
    std::unique_ptr<WebServiceWorkerInstalledScriptsManagerParams>
        installed_scripts_manager_params,
    mojo::ScopedMessagePipeHandle content_settings_handle,
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner) {
  DCHECK(!asked_to_terminate_);
  worker_start_data_ = data;

  std::unique_ptr<ServiceWorkerInstalledScriptsManager>
      installed_scripts_manager;
  if (installed_scripts_manager_params) {
    DCHECK(installed_scripts_manager_params->manager_receiver.is_valid());
    DCHECK(installed_scripts_manager_params->manager_host_remote.is_valid());
    Vector<KURL> installed_scripts_urls;
    installed_scripts_urls.AppendRange(
        installed_scripts_manager_params->installed_scripts_urls.begin(),
        installed_scripts_manager_params->installed_scripts_urls.end());
    installed_scripts_manager = std::make_unique<
        ServiceWorkerInstalledScriptsManager>(
        installed_scripts_urls,
        mojo::PendingReceiver<
            mojom::blink::ServiceWorkerInstalledScriptsManager>(
            std::move(installed_scripts_manager_params->manager_receiver)),
        mojo::PendingRemote<
            mojom::blink::ServiceWorkerInstalledScriptsManagerHost>(
            std::move(installed_scripts_manager_params->manager_host_remote),
            mojom::blink::ServiceWorkerInstalledScriptsManagerHost::Version_),
        Platform::Current()->GetIOTaskRunner());
  }

  // TODO(mkwst): This really needs to be piped through from the requesting
  // document, like we're doing for SharedWorkers. That turns out to be
  // incredibly convoluted, and since ServiceWorkers are locked to the same
  // origin as the page which requested them, the only time it would come
  // into play is a DNS poisoning attack after the page load. It's something
  // we should fix, but we're taking this shortcut for the prototype.
  //
  // https://crbug.com/590714
  KURL script_url = worker_start_data_.script_url;
  worker_start_data_.address_space = network::mojom::IPAddressSpace::kPublic;
  if (network_utils::IsReservedIPAddress(script_url.Host()))
    worker_start_data_.address_space = network::mojom::IPAddressSpace::kPrivate;
  if (SecurityOrigin::Create(script_url)->IsLocalhost())
    worker_start_data_.address_space = network::mojom::IPAddressSpace::kLocal;

  devtools_worker_token_ = data.devtools_worker_token;
  wait_for_debugger_mode_ = worker_start_data_.wait_for_debugger_mode;
  StartWorkerThread(
      std::move(installed_scripts_manager),
      std::make_unique<ServiceWorkerContentSettingsProxy>(
          // Chrome doesn't use interface versioning.
          // TODO(falken): Is that comment about versioning correct?
          mojo::PendingRemote<mojom::blink::WorkerContentSettingsProxy>(
              std::move(content_settings_handle), 0u)),
      std::move(initiator_thread_task_runner));
}

void WebEmbeddedWorkerImpl::TerminateWorkerContext() {
  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;
  if (!worker_thread_) {
    // The worker thread has not been created yet if the worker is asked to
    // terminate during waiting for debugger.
    DCHECK_EQ(WebEmbeddedWorkerStartData::kWaitForDebugger,
              worker_start_data_.wait_for_debugger_mode);
    // This deletes 'this'.
    worker_context_client_->WorkerContextFailedToStartOnInitiatorThread();
    return;
  }
  worker_thread_->Terminate();
}

void WebEmbeddedWorkerImpl::ResumeAfterDownload() {
  // TODO(bashi): Remove this method. This does nothing anymore.
  DCHECK(!asked_to_terminate_);
}

void WebEmbeddedWorkerImpl::StartWorkerThread(
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager,
    std::unique_ptr<ServiceWorkerContentSettingsProxy> content_settings_proxy,
    scoped_refptr<base::SingleThreadTaskRunner> initiator_thread_task_runner) {
  DCHECK(!asked_to_terminate_);

  // For now we don't use global scope name for service workers.
  const String global_scope_name = g_empty_string;

  // TODO(crbug.com/967265,937177): Plumb these starter parameters from an
  // appropriate Document. See comment in CreateFetchClientSettingsObject() for
  // details.
  scoped_refptr<const SecurityOrigin> starter_origin =
      SecurityOrigin::Create(worker_start_data_.script_url);
  // This roughly equals to shadow document's IsSecureContext() as a shadow
  // document have a frame with no parent.
  // See also Document::InitSecureContextState().
  bool starter_secure_context =
      starter_origin->IsPotentiallyTrustworthy() ||
      SchemeRegistry::SchemeShouldBypassSecureContextCheck(
          starter_origin->Protocol());
  const HttpsState starter_https_state =
      CalculateHttpsState(starter_origin.get());

  scoped_refptr<WebWorkerFetchContext> web_worker_fetch_context =
      worker_context_client_->CreateWorkerFetchContextOnInitiatorThread();

  // Create WorkerSettings. Currently we block all mixed-content requests from
  // a ServiceWorker.
  // TODO(bashi): Set some of these settings from WebPreferences. We may want
  // to propagate and update these settings from the browser process in a way
  // similar to mojom::RendererPreference{Watcher}.
  auto worker_settings = std::make_unique<WorkerSettings>(
      false /* disable_reading_from_canvas */,
      true /* strict_mixed_content_checking */,
      false /* allow_running_of_insecure_content */,
      false /* strictly_block_blockable_mixed_content */,
      GenericFontFamilySettings());

  std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params;
  String source_code;
  std::unique_ptr<Vector<uint8_t>> cached_meta_data;

  bool is_script_installed =
      installed_scripts_manager && installed_scripts_manager->IsScriptInstalled(
                                       worker_start_data_.script_url);

  // We don't have to set ContentSecurityPolicy and ReferrerPolicy. They're
  // served by the worker script loader or the installed scripts manager on the
  // worker thread.
  global_scope_creation_params = std::make_unique<GlobalScopeCreationParams>(
      worker_start_data_.script_url, worker_start_data_.script_type,
      OffMainThreadWorkerScriptFetchOption::kEnabled, global_scope_name,
      worker_start_data_.user_agent, std::move(web_worker_fetch_context),
      Vector<CSPHeaderAndType>(), network::mojom::ReferrerPolicy::kDefault,
      starter_origin.get(), starter_secure_context, starter_https_state,
      nullptr /* worker_clients */, std::move(content_settings_proxy),
      base::nullopt /* response_address_space */,
      nullptr /* OriginTrialTokens */, devtools_worker_token_,
      std::move(worker_settings),
      static_cast<V8CacheOptions>(worker_start_data_.v8_cache_options),
      nullptr /* worklet_module_respones_map */,
      std::move(interface_provider_info_), std::move(browser_interface_broker_),
      BeginFrameProviderParams(), nullptr /* parent_feature_policy */,
      base::UnguessableToken() /* agent_cluster_id */);

  // Generate the full code cache in the first execution of the script.
  global_scope_creation_params->v8_cache_options =
      kV8CacheOptionsFullCodeWithoutHeatCheck;

  worker_thread_ = std::make_unique<ServiceWorkerThread>(
      std::make_unique<ServiceWorkerGlobalScopeProxy>(
          *this, *worker_context_client_, initiator_thread_task_runner),
      std::move(installed_scripts_manager), std::move(cache_storage_remote_),
      initiator_thread_task_runner);

  auto devtools_params = std::make_unique<WorkerDevToolsParams>();
  devtools_params->devtools_worker_token = devtools_worker_token_;
  devtools_params->wait_for_debugger =
      wait_for_debugger_mode_ == WebEmbeddedWorkerStartData::kWaitForDebugger;
  mojo::PendingRemote<mojom::blink::DevToolsAgent> devtools_agent_remote;
  devtools_params->agent_receiver =
      devtools_agent_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<mojom::blink::DevToolsAgentHost>
      devtools_agent_host_receiver =
          devtools_params->agent_host_remote.InitWithNewPipeAndPassReceiver();

  worker_thread_->Start(std::move(global_scope_creation_params),
                        WorkerBackingThreadStartupData::CreateDefault(),
                        std::move(devtools_params));

  // If this is an installed service worker, the installed script will be read
  // from the service worker script storage on the worker thread.
  if (is_script_installed) {
    switch (worker_start_data_.script_type) {
      case mojom::ScriptType::kClassic:
        worker_thread_->RunInstalledClassicScript(
            worker_start_data_.script_url, v8_inspector::V8StackTraceId());
        break;
      case mojom::ScriptType::kModule:
        worker_thread_->RunInstalledModuleScript(
            worker_start_data_.script_url,
            CreateFetchClientSettingsObjectData(starter_origin.get(),
                                                starter_https_state),
            network::mojom::CredentialsMode::kOmit);
        break;
    }
  } else {
    std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
        fetch_client_setting_object_data = CreateFetchClientSettingsObjectData(
            starter_origin.get(), starter_https_state);

    // If this is a new (not installed) service worker, we are in the Update
    // algorithm here:
    // > Switching on job's worker type, run these substeps with the following
    // > options:
    // https://w3c.github.io/ServiceWorker/#update-algorithm
    switch (worker_start_data_.script_type) {
      // > "classic": Fetch a classic worker script given job's serialized
      // > script url, job's client, "serviceworker", and the to-be-created
      // > environment settings object for this service worker.
      case mojom::ScriptType::kClassic:
        worker_thread_->FetchAndRunClassicScript(
            worker_start_data_.script_url,
            std::move(fetch_client_setting_object_data),
            nullptr /* outside_resource_timing_notifier */,
            v8_inspector::V8StackTraceId());
        break;

      // > "module": Fetch a module worker script graph given job’s serialized
      // > script url, job’s client, "serviceworker", "omit", and the
      // > to-be-created environment settings object for this service worker.
      case mojom::ScriptType::kModule:
        worker_thread_->FetchAndRunModuleScript(
            worker_start_data_.script_url,
            std::move(fetch_client_setting_object_data),
            nullptr /* outside_resource_timing_notifier */,
            network::mojom::CredentialsMode::kOmit);
        break;
    }
  }
  // We are now ready to inspect worker thread.
  worker_context_client_->WorkerReadyForInspectionOnInitiatorThread(
      devtools_agent_remote.PassPipe(),
      devtools_agent_host_receiver.PassPipe());
}

std::unique_ptr<CrossThreadFetchClientSettingsObjectData>
WebEmbeddedWorkerImpl::CreateFetchClientSettingsObjectData(
    const SecurityOrigin* security_origin,
    const HttpsState& https_state) {
  // TODO(crbug.com/967265): Currently we create an incomplete outside settings
  // object from |worker_start_data_| but we should create a proper outside
  // settings objects depending on the situation. For new worker case, this
  // should be the Document that called navigator.serviceWorker.register(). For
  // ServiceWorkerRegistration#update() case, it should be the Document that
  // called update(). For soft update case, it seems to be 'null' document.
  //
  // To get a correct settings, we need to make a way to pass the settings
  // object over mojo IPCs.

  const KURL& script_url = worker_start_data_.script_url;
  return std::make_unique<CrossThreadFetchClientSettingsObjectData>(
      script_url.Copy() /* global_object_url */,
      script_url.Copy() /* base_url */, security_origin->IsolatedCopy(),
      network::mojom::ReferrerPolicy::kDefault,
      script_url.GetString().IsolatedCopy() /* outgoing_referrer */,
      https_state, AllowedByNosniff::MimeTypeCheck::kLax,
      worker_start_data_.address_space,
      kBlockAllMixedContent /* insecure_requests_policy */,
      FetchClientSettingsObject::InsecureNavigationsSet(),
      false /* mixed_autoupgrade_opt_out */);
}

void WebEmbeddedWorkerImpl::WaitForShutdownForTesting() {
  DCHECK(worker_thread_);
  worker_thread_->WaitForShutdownForTesting();
}

}  // namespace blink
