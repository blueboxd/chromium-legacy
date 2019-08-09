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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_

#include "third_party/blink/public/web/web_shared_worker.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/ip_address_space.mojom-blink.h"
#include "services/service_manager/public/mojom/interface_provider.mojom-blink.h"
#include "third_party/blink/public/common/privacy_preferences.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/csp/content_security_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-blink.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/exported/worker_shadow_page.h"
#include "third_party/blink/renderer/core/loader/appcache/application_cache_host_for_shared_worker.h"
#include "third_party/blink/renderer/core/workers/shared_worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace blink {

class FetchClientSettingsObjectSnapshot;
class SharedWorkerThread;
class WebSharedWorkerClient;
class WebString;
class WebURL;

// This class is used by the worker process code to talk to the SharedWorker
// implementation. This is basically accessed on the main thread, but some
// methods must be called from a worker thread. Such methods are suffixed with
// *OnWorkerThread or have header comments.
//
// Owned by WebSharedWorkerClient. Destroyed in TerminateWorkerThread() or
// DidTerminateWorkerThread() via
// WebSharedWorkerClient::WorkerContextDestroyed().
class CORE_EXPORT WebSharedWorkerImpl final : public WebSharedWorker,
                                              public WorkerShadowPage::Client {
 public:
  WebSharedWorkerImpl(WebSharedWorkerClient*,
                      const base::UnguessableToken& appcache_host_id);
  ~WebSharedWorkerImpl() override;

  // WorkerShadowPage::Client overrides.
  void OnShadowPageInitialized() override;
  WebLocalFrameClient::AppCacheType GetAppCacheType() override {
    return WebLocalFrameClient::AppCacheType::kAppCacheForSharedWorker;
  }

  // WebSharedWorker methods:
  void StartWorkerContext(
      const WebURL&,
      const WebString& name,
      const WebString& user_agent,
      const WebString& content_security_policy,
      mojom::ContentSecurityPolicyType,
      network::mojom::IPAddressSpace,
      const base::UnguessableToken& devtools_worker_token,
      PrivacyPreferences privacy_preferences,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      mojo::ScopedMessagePipeHandle content_settings_handle,
      mojo::ScopedMessagePipeHandle interface_provider,
      mojo::ScopedMessagePipeHandle browser_interface_broker) override;
  void Connect(MessagePortChannel) override;
  void TerminateWorkerContext() override;
  void PauseWorkerContextOnStart() override;

  // Callback methods for SharedWorkerReportingProxy.
  void CountFeature(WebFeature);
  void DidFetchScript(int64_t app_cache_id);
  void DidFailToFetchClassicScript();
  void DidEvaluateClassicScript(bool success);
  void DidCloseWorkerGlobalScope();
  // This synchronously destroys |this|.
  void DidTerminateWorkerThread();

 private:
  SharedWorkerThread* GetWorkerThread() { return worker_thread_.get(); }

  // Shuts down the worker thread. This may synchronously destroy |this|.
  void TerminateWorkerThread();

  void OnAppCacheSelected();
  void ContinueStartWorkerContext();
  void StartWorkerThread(
      std::unique_ptr<GlobalScopeCreationParams>,
      const KURL& script_response_url,
      const String& source_code,
      const FetchClientSettingsObjectSnapshot& outside_settings_object);
  WorkerClients* CreateWorkerClients();

  void ConnectTaskOnWorkerThread(MessagePortChannel);

  std::unique_ptr<WorkerShadowPage> shadow_page_;
  // Unique worker token used by DevTools to attribute different instrumentation
  // to the same worker.
  base::UnguessableToken devtools_worker_token_;

  Persistent<SharedWorkerReportingProxy> reporting_proxy_;
  std::unique_ptr<SharedWorkerThread> worker_thread_;
  mojom::blink::WorkerContentSettingsProxyPtrInfo content_settings_info_;

  // |client_| owns |this|.
  WebSharedWorkerClient* client_;

  bool asked_to_terminate_ = false;
  bool pause_worker_context_on_start_ = false;

  WebURL script_request_url_;
  WebString name_;
  WebString user_agent_;
  network::mojom::IPAddressSpace creation_address_space_;

  // TODO(crbug.com/990845): remove when no longer used.
  service_manager::mojom::blink::InterfaceProviderPtrInfo
      pending_interface_provider_;

  mojo::PendingRemote<mojom::blink::BrowserInterfaceBroker>
      browser_interface_broker_;

  Persistent<ApplicationCacheHostForSharedWorker> appcache_host_;

  base::WeakPtrFactory<WebSharedWorkerImpl> weak_ptr_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EXPORTED_WEB_SHARED_WORKER_IMPL_H_
