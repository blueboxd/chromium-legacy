// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_network_contexts.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "chromecast/base/cast_features.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_http_user_agent_settings.h"
#include "chromecast/browser/url_request_context_factory.h"
#include "chromecast/common/cast_content_client.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace chromecast {
namespace shell {

// SharedURLLoaderFactory backed by a CastNetworkContexts and its system
// NetworkContext. Transparently handles crashes.
class CastNetworkContexts::URLLoaderFactoryForSystem
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForSystem(CastNetworkContexts* network_context)
      : network_context_(network_context) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // mojom::URLLoaderFactory implementation:
  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!network_context_)
      return;
    network_context_->GetSystemURLLoaderFactory()->CreateLoaderAndStart(
        std::move(request), routing_id, request_id, options, url_request,
        std::move(client), traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    if (!network_context_)
      return;
    network_context_->GetSystemURLLoaderFactory()->Clone(std::move(request));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
        this);
  }

  void Shutdown() { network_context_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForSystem>;
  ~URLLoaderFactoryForSystem() override {}

  SEQUENCE_CHECKER(sequence_checker_);
  CastNetworkContexts* network_context_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForSystem);
};

CastNetworkContexts::CastNetworkContexts(
    std::vector<std::string> cors_exempt_headers_list)
    : cors_exempt_headers_list_(std::move(cors_exempt_headers_list)),
      system_shared_url_loader_factory_(
          base::MakeRefCounted<URLLoaderFactoryForSystem>(this)) {}

CastNetworkContexts::~CastNetworkContexts() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  system_shared_url_loader_factory_->Shutdown();
}

network::mojom::NetworkContext* CastNetworkContexts::GetSystemContext() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!system_network_context_ || system_network_context_.encountered_error()) {
    // This should call into OnNetworkServiceCreated(), which will re-create
    // the network service, if needed. There's a chance that it won't be
    // invoked, if the NetworkContext has encountered an error but the
    // NetworkService has not yet noticed its pipe was closed. In that case,
    // trying to create a new NetworkContext would fail, anyways, and hopefully
    // a new NetworkContext will be created on the next GetContext() call.
    content::GetNetworkService();
    DCHECK(system_network_context_);
  }

  return system_network_context_.get();
}

network::mojom::URLLoaderFactory*
CastNetworkContexts::GetSystemURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Create the URLLoaderFactory as needed.
  if (system_url_loader_factory_ &&
      !system_url_loader_factory_.encountered_error()) {
    return system_url_loader_factory_.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = false;
  GetSystemContext()->CreateURLLoaderFactory(
      mojo::MakeRequest(&system_url_loader_factory_), std::move(params));
  return system_shared_url_loader_factory_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
CastNetworkContexts::GetSystemSharedURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return system_shared_url_loader_factory_;
}

network::mojom::NetworkContextPtr CastNetworkContexts::CreateNetworkContext(
    content::BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  network::mojom::NetworkContextPtr network_context;
  network::mojom::NetworkContextParamsPtr context_params =
      CreateDefaultNetworkContextParams();

  content::UpdateCorsExemptHeader(context_params.get());

  // Copy of what's in ContentBrowserClient::CreateNetworkContext for now.
  context_params->accept_language = "en-us,en";

  content::GetNetworkService()->CreateNetworkContext(
      MakeRequest(&network_context), std::move(context_params));
  return network_context;
}

void CastNetworkContexts::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Disable QUIC if instructed by DCS. This remains constant for the lifetime
  // of the process.
  if (!chromecast::IsFeatureEnabled(kEnableQuic))
    network_service->DisableQuic();

  // The system NetworkContext must be created first, since it sets
  // |primary_network_context| to true.
  network_service->CreateNetworkContext(MakeRequest(&system_network_context_),
                                        CreateSystemNetworkContextParams());
}

void CastNetworkContexts::OnLocaleUpdate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto accept_language = CastHttpUserAgentSettings::AcceptLanguage();

  GetSystemContext()->SetAcceptLanguage(accept_language);

  auto* browser_context = CastBrowserProcess::GetInstance()->browser_context();
  content::BrowserContext::GetDefaultStoragePartition(browser_context)
      ->GetNetworkContext()
      ->SetAcceptLanguage(accept_language);
}

void CastNetworkContexts::OnPrefServiceShutdown() {
  if (proxy_config_service_)
    proxy_config_service_->RemoveObserver(this);

  if (pref_proxy_config_tracker_impl_)
    pref_proxy_config_tracker_impl_->DetachFromPrefService();
}

network::mojom::NetworkContextParamsPtr
CastNetworkContexts::CreateDefaultNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();

  network_context_params->user_agent = GetUserAgent();
  network_context_params->accept_language =
      CastHttpUserAgentSettings::AcceptLanguage();

  // Disable idle sockets close on memory pressure, if instructed by DCS. On
  // memory constrained devices:
  // 1. if idle sockets are closed when memory pressure happens, cast_shell will
  // close and re-open lots of connections to server.
  // 2. if idle sockets are kept alive when memory pressure happens, this may
  // cause JS engine gc frequently, leading to JS suspending.
  network_context_params->disable_idle_sockets_close_on_memory_pressure =
      IsFeatureEnabled(kDisableIdleSocketsCloseOnMemoryPressure);

  AddProxyToNetworkContextParams(network_context_params.get());

  network_context_params->cors_exempt_header_list = cors_exempt_headers_list_;

  return network_context_params;
}

network::mojom::NetworkContextParamsPtr
CastNetworkContexts::CreateSystemNetworkContextParams() {
  network::mojom::NetworkContextParamsPtr network_context_params =
      CreateDefaultNetworkContextParams();
  content::UpdateCorsExemptHeader(network_context_params.get());

  network_context_params->context_name = std::string("system");

  network_context_params->primary_network_context = true;

  return network_context_params;
}

void CastNetworkContexts::AddProxyToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  if (!proxy_config_service_) {
    pref_proxy_config_tracker_impl_ =
        std::make_unique<PrefProxyConfigTrackerImpl>(
            CastBrowserProcess::GetInstance()->pref_service(), nullptr);
    proxy_config_service_ =
        pref_proxy_config_tracker_impl_->CreateTrackingProxyConfigService(
            nullptr);
    proxy_config_service_->AddObserver(this);
  }

  network::mojom::ProxyConfigClientPtr proxy_config_client;
  network_context_params->proxy_config_client_request =
      mojo::MakeRequest(&proxy_config_client);
  proxy_config_client_set_.AddPtr(std::move(proxy_config_client));

  poller_binding_set_.AddBinding(
      this,
      mojo::MakeRequest(&network_context_params->proxy_config_poller_client));

  net::ProxyConfigWithAnnotation proxy_config;
  net::ProxyConfigService::ConfigAvailability availability =
      proxy_config_service_->GetLatestProxyConfig(&proxy_config);
  if (availability != net::ProxyConfigService::CONFIG_PENDING)
    network_context_params->initial_proxy_config = proxy_config;
}

void CastNetworkContexts::OnProxyConfigChanged(
    const net::ProxyConfigWithAnnotation& config,
    net::ProxyConfigService::ConfigAvailability availability) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_config_client_set_.ForAllPtrs(
      [config,
       availability](network::mojom::ProxyConfigClient* proxy_config_client) {
        switch (availability) {
          case net::ProxyConfigService::CONFIG_VALID:
            proxy_config_client->OnProxyConfigUpdated(config);
            break;
          case net::ProxyConfigService::CONFIG_UNSET:
            proxy_config_client->OnProxyConfigUpdated(
                net::ProxyConfigWithAnnotation::CreateDirect());
            break;
          case net::ProxyConfigService::CONFIG_PENDING:
            NOTREACHED();
            break;
        }
      });
}

void CastNetworkContexts::OnLazyProxyConfigPoll() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_config_service_->OnLazyPoll();
}

}  // namespace shell
}  // namespace chromecast
