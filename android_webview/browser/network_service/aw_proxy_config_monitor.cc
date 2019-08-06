// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxy_config_monitor.h"

#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"

namespace android_webview {

namespace {
const char kProxyServerSwitch[] = "proxy-server";
const char kProxyBypassListSwitch[] = "proxy-bypass-list";

base::LazyInstance<AwProxyConfigMonitor>::Leaky g_instance;
}  // namespace

AwProxyConfigMonitor::AwProxyConfigMonitor() {
  proxy_config_service_android_ =
      std::make_unique<net::ProxyConfigServiceAndroid>(
          base::ThreadTaskRunnerHandle::Get(),
          base::ThreadTaskRunnerHandle::Get());
  proxy_config_service_android_->set_exclude_pac_url(true);
  proxy_config_service_android_->AddObserver(this);
}

AwProxyConfigMonitor::~AwProxyConfigMonitor() {
  proxy_config_service_android_->RemoveObserver(this);
}

AwProxyConfigMonitor* AwProxyConfigMonitor::GetInstance() {
  return g_instance.Pointer();
}

void AwProxyConfigMonitor::AddProxyToNetworkContextParams(
    network::mojom::NetworkContextParamsPtr& network_context_params) {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kProxyServerSwitch)) {
    std::string proxy = command_line.GetSwitchValueASCII(kProxyServerSwitch);
    net::ProxyConfig proxy_config;
    proxy_config.proxy_rules().ParseFromString(proxy);
    if (command_line.HasSwitch(kProxyBypassListSwitch)) {
      std::string bypass_list =
          command_line.GetSwitchValueASCII(kProxyBypassListSwitch);
      proxy_config.proxy_rules().bypass_rules.ParseFromString(bypass_list);
    }

    network_context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation(proxy_config, NO_TRAFFIC_ANNOTATION_YET);
  } else {
    network::mojom::ProxyConfigClientPtr proxy_config_client;
    network_context_params->proxy_config_client_request =
        mojo::MakeRequest(&proxy_config_client);
    proxy_config_client_set_.AddPtr(std::move(proxy_config_client));

    net::ProxyConfigWithAnnotation proxy_config;
    net::ProxyConfigService::ConfigAvailability availability =
        proxy_config_service_android_->GetLatestProxyConfig(&proxy_config);
    if (availability == net::ProxyConfigService::CONFIG_VALID)
      network_context_params->initial_proxy_config = proxy_config;
  }
}

void AwProxyConfigMonitor::OnProxyConfigChanged(
    const net::ProxyConfigWithAnnotation& config,
    net::ProxyConfigService::ConfigAvailability availability) {
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

std::string AwProxyConfigMonitor::SetProxyOverride(
    const std::vector<net::ProxyConfigServiceAndroid::ProxyOverrideRule>&
        proxy_rules,
    const std::vector<std::string>& bypass_rules,
    base::OnceClosure callback) {
  return proxy_config_service_android_->SetProxyOverride(
      proxy_rules, bypass_rules,
      base::BindOnce(&AwProxyConfigMonitor::FlushProxyConfig,
                     base::Unretained(this), std::move(callback)));
}

void AwProxyConfigMonitor::ClearProxyOverride(base::OnceClosure callback) {
  proxy_config_service_android_->ClearProxyOverride(
      base::BindOnce(&AwProxyConfigMonitor::FlushProxyConfig,
                     base::Unretained(this), std::move(callback)));
}

void AwProxyConfigMonitor::FlushProxyConfig(base::OnceClosure callback) {
  int count = 0;
  proxy_config_client_set_.ForAllPtrs(
      [&count](network::mojom::ProxyConfigClient* proxy_config_client) {
        ++count;
      });

  base::RepeatingClosure closure =
      base::BarrierClosure(count, std::move(callback));
  proxy_config_client_set_.ForAllPtrs(
      [closure](network::mojom::ProxyConfigClient* proxy_config_client) {
        proxy_config_client->FlushProxyConfig(closure);
      });
}

}  // namespace android_webview
