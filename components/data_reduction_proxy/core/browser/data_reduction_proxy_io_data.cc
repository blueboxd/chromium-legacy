// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_throttle_manager.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace data_reduction_proxy {

DataReductionProxyIOData::DataReductionProxyIOData(
    Client client,
    PrefService* prefs,
    network::NetworkConnectionTracker* network_connection_tracker,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    bool enabled,
    const std::string& user_agent,
    const std::string& channel)
    : client_(client),
      network_connection_tracker_(network_connection_tracker),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      enabled_(enabled),
      channel_(channel),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  DCHECK(io_task_runner_);
  DCHECK(ui_task_runner_);
  configurator_.reset(new DataReductionProxyConfigurator());
  configurator_->SetConfigUpdatedCallback(base::BindRepeating(
      &DataReductionProxyIOData::OnProxyConfigUpdated, base::Unretained(this)));
  DataReductionProxyMutableConfigValues* raw_mutable_config = nullptr;
    std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config =
        std::make_unique<DataReductionProxyMutableConfigValues>();
    raw_mutable_config = mutable_config.get();
    config_.reset(new DataReductionProxyConfig(
        io_task_runner, ui_task_runner, network_connection_tracker_,
        std::move(mutable_config), configurator_.get()));
    request_options_.reset(
        new DataReductionProxyRequestOptions(client_, config_.get()));
    request_options_->Init();
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives the caller (since the
    // caller is owned by |this|.
    request_options_->SetUpdateHeaderCallback(base::BindRepeating(
        &DataReductionProxyIOData::UpdateProxyRequestHeaders,
        base::Unretained(this)));

    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives the caller (since the
    // caller is owned by |this|.
    if (!params::IsIncludedInHoldbackFieldTrial()) {
      config_client_.reset(new DataReductionProxyConfigServiceClient(
          GetBackoffPolicy(), request_options_.get(), raw_mutable_config,
          config_.get(), this, network_connection_tracker_,
          base::BindRepeating(&DataReductionProxyIOData::StoreSerializedConfig,
                              base::Unretained(this))));
    }

    network_properties_manager_.reset(new NetworkPropertiesManager(
        base::DefaultClock::GetInstance(), prefs, ui_task_runner_));
}

DataReductionProxyIOData::DataReductionProxyIOData(
    PrefService* prefs,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : client_(Client::UNKNOWN),
      network_connection_tracker_(nullptr),
      io_task_runner_(io_task_runner),
      ui_task_runner_(ui_task_runner),
      effective_connection_type_(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN) {
  DCHECK(ui_task_runner_);
  DCHECK(io_task_runner_);
  network_properties_manager_.reset(new NetworkPropertiesManager(
      base::DefaultClock::GetInstance(), prefs, ui_task_runner_));
}

DataReductionProxyIOData::~DataReductionProxyIOData() {
  // Guaranteed to be destroyed on IO thread if the IO thread is still
  // available at the time of destruction. If the IO thread is unavailable,
  // then the destruction will happen on the UI thread.
}

void DataReductionProxyIOData::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  network_properties_manager_->ShutdownOnUIThread();
}

void DataReductionProxyIOData::SetDataReductionProxyService(
    base::WeakPtr<DataReductionProxyService> data_reduction_proxy_service,
    const std::string& user_agent) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  service_ = data_reduction_proxy_service;
  url_loader_factory_info_ = service_->url_loader_factory_info();
  // Using base::Unretained is safe here, unless the browser is being shut down
  // before the Initialize task can be executed. The task is only created as
  // part of class initialization.
  if (io_task_runner_->BelongsToCurrentThread()) {
    InitializeOnIOThread(user_agent);
    return;
  }
  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyIOData::InitializeOnIOThread,
                                base::Unretained(this), user_agent));
}

void DataReductionProxyIOData::InitializeOnIOThread(
    const std::string& user_agent) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  DCHECK(network_properties_manager_);

  DCHECK(url_loader_factory_info_);
  auto url_loader_factory = network::SharedURLLoaderFactory::Create(
      std::move(url_loader_factory_info_));

  config_->InitializeOnIOThread(
      url_loader_factory,
      base::BindRepeating(&DataReductionProxyIOData::CreateCustomProxyConfig,
                          base::Unretained(this), true),
      network_properties_manager_.get(), user_agent);
  if (config_client_)
    config_client_->InitializeOnIOThread(url_loader_factory);
  if (ui_task_runner_->BelongsToCurrentThread()) {
    service_->SetIOData(weak_factory_.GetWeakPtr());
    return;
  }
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetIOData, service_,
                                weak_factory_.GetWeakPtr()));
}

bool DataReductionProxyIOData::IsEnabled() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return enabled_;
}

void DataReductionProxyIOData::SetPingbackReportingFraction(
    float pingback_reporting_fraction) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::SetPingbackReportingFraction,
                     service_, pingback_reporting_fraction));
}

void DataReductionProxyIOData::DeleteBrowsingHistory(const base::Time start,
                                                     const base::Time end) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  network_properties_manager_->DeleteHistory();
}

void DataReductionProxyIOData::OnCacheCleared(const base::Time start,
                                              const base::Time end) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  network_properties_manager_->DeleteHistory();
}

// TODO(kundaji): Rename this method to something more descriptive.
// Bug http://crbug/488190.
void DataReductionProxyIOData::SetProxyPrefs(bool enabled, bool at_startup) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  enabled_ = enabled;
  config_->SetProxyConfig(enabled, at_startup);
  if (config_client_) {
    config_client_->SetEnabled(enabled);
    if (enabled)
      config_client_->RetrieveConfig();
  }

  // If Data Saver is disabled, reset data reduction proxy state.
  if (!enabled) {
    if (proxy_config_client_)
      proxy_config_client_->ClearBadProxiesCache();
  }
}

void DataReductionProxyIOData::SetDataReductionProxyConfiguration(
    const std::string& serialized_config) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (config_client_)
    config_client_->ApplySerializedConfig(serialized_config);
}

void DataReductionProxyIOData::SetIgnoreLongTermBlackListRules(
    bool ignore_long_term_black_list_rules) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataReductionProxyService::SetIgnoreLongTermBlackListRules, service_,
          ignore_long_term_black_list_rules));
}

void DataReductionProxyIOData::UpdateDataUseForHost(int64_t network_bytes,
                                                    int64_t original_bytes,
                                                    const std::string& host) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::UpdateDataUseForHost, service_,
                     network_bytes, original_bytes, host));
}

void DataReductionProxyIOData::UpdateContentLengths(
    int64_t data_used,
    int64_t original_size,
    bool data_reduction_proxy_enabled,
    DataReductionProxyRequestType request_type,
    const std::string& mime_type,
    bool is_user_traffic,
    data_use_measurement::DataUseUserData::DataUseContentType content_type,
    int32_t service_hash_code) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindRepeating(&DataReductionProxyService::UpdateContentLengths,
                          service_, data_used, original_size,
                          data_reduction_proxy_enabled, request_type, mime_type,
                          is_user_traffic, content_type, service_hash_code));
}

void DataReductionProxyIOData::SetUnreachable(bool unreachable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetUnreachable,
                                service_, unreachable));
}

void DataReductionProxyIOData::SetInt64Pref(const std::string& pref_path,
                                            int64_t value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetInt64Pref,
                                service_, pref_path, value));
}

void DataReductionProxyIOData::SetStringPref(const std::string& pref_path,
                                             const std::string& value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DataReductionProxyService::SetStringPref,
                                service_, pref_path, value));
}

void DataReductionProxyIOData::StoreSerializedConfig(
    const std::string& serialized_config) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  SetStringPref(prefs::kDataReductionProxyConfig, serialized_config);
  SetInt64Pref(prefs::kDataReductionProxyLastConfigRetrievalTime,
               (base::Time::Now() - base::Time()).InMicroseconds());
}

void DataReductionProxyIOData::UpdateProxyRequestHeaders(
    const net::HttpRequestHeaders& headers) {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::SetProxyRequestHeadersOnUI,
                     service_, std::move(headers)));
  UpdateCustomProxyConfig();
}

void DataReductionProxyIOData::OnProxyConfigUpdated() {
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DataReductionProxyService::SetConfiguredProxiesOnUI,
                     service_, config_->GetAllConfiguredProxies(),
                     config_->GetProxiesForHttp()));
  UpdateCustomProxyConfig();
  UpdateThrottleConfig();
}

network::mojom::CustomProxyConfigPtr
DataReductionProxyIOData::CreateCustomProxyConfig(
    bool is_warmup_url,
    const std::vector<DataReductionProxyServer>& proxies_for_http) const {
  auto config = network::mojom::CustomProxyConfig::New();
  if (params::IsIncludedInHoldbackFieldTrial()) {
    config->rules =
        configurator_
            ->CreateProxyConfig(is_warmup_url,
                                config_->GetNetworkPropertiesManager(),
                                std::vector<DataReductionProxyServer>())
            .proxy_rules();
  } else {
    config->rules =
        configurator_
            ->CreateProxyConfig(is_warmup_url,
                                config_->GetNetworkPropertiesManager(),
                                proxies_for_http)
            .proxy_rules();
  }

  net::EffectiveConnectionType type = GetEffectiveConnectionType();
  if (type > net::EFFECTIVE_CONNECTION_TYPE_OFFLINE) {
    DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_LAST, type);
    config->pre_cache_headers.SetHeader(
        chrome_proxy_ect_header(),
        net::GetNameForEffectiveConnectionType(type));
  }

  request_options_->AddRequestHeader(&config->post_cache_headers,
                                     base::nullopt);

  config->assume_https_proxies_support_quic = true;
  config->can_use_proxy_on_http_url_redirect_cycles = false;

  return config;
}

void DataReductionProxyIOData::UpdateCustomProxyConfig() {
  if (!proxy_config_client_)
    return;

  proxy_config_client_->OnCustomProxyConfigUpdated(CreateCustomProxyConfig(
      !base::FeatureList::IsEnabled(
          features::kDataReductionProxyDisableProxyFailedWarmup),
      config_->GetProxiesForHttp()));
}

void DataReductionProxyIOData::UpdateThrottleConfig() {
  if (drp_throttle_config_observers_.empty())
    return;

  auto config = CreateThrottleConfig();

  drp_throttle_config_observers_.ForAllPtrs(
      [&config](mojom::DataReductionProxyThrottleConfigObserver* observer) {
        observer->OnThrottleConfigChanged(config->Clone());
      });
}

mojom::DataReductionProxyThrottleConfigPtr
DataReductionProxyIOData::CreateThrottleConfig() const {
  return DataReductionProxyThrottleManager::CreateConfig(
      config_->GetProxiesForHttp());
}

void DataReductionProxyIOData::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType type) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  effective_connection_type_ = type;
  UpdateCustomProxyConfig();
}

void DataReductionProxyIOData::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  config_->OnRTTOrThroughputEstimatesComputed(http_rtt);
}

net::EffectiveConnectionType
DataReductionProxyIOData::GetEffectiveConnectionType() const {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return effective_connection_type_;
}

void DataReductionProxyIOData::SetCustomProxyConfigClient(
    network::mojom::CustomProxyConfigClientPtrInfo config_client_info) {
  proxy_config_client_.Bind(std::move(config_client_info));
  UpdateCustomProxyConfig();
}

void DataReductionProxyIOData::MarkProxiesAsBad(
    base::TimeDelta bypass_duration,
    const net::ProxyList& bad_proxies,
    mojom::DataReductionProxy::MarkProxiesAsBadCallback callback) {
  // Sanity check the inputs, as this data may originate from a lower-privilege
  // process (renderer).

  if (bypass_duration < base::TimeDelta()) {
    LOG(ERROR) << "Received bad MarkProxiesAsBad() -- invalid bypass_duration: "
               << bypass_duration;
    std::move(callback).Run();
    return;
  }

  // Limit maximum bypass duration to a day.
  if (bypass_duration > base::TimeDelta::FromDays(1))
    bypass_duration = base::TimeDelta::FromDays(1);

  // |bad_proxies| should be DRP servers or this API allows marking arbitrary
  // proxies as bad. It is possible that proxies from an older config are
  // received (FindConfiguredDataReductionProxy() searches recent proxies too).
  for (const auto& proxy : bad_proxies.GetAll()) {
    if (!config_->FindConfiguredDataReductionProxy(proxy)) {
      LOG(ERROR) << "Received bad MarkProxiesAsBad() -- not a DRP server: "
                 << proxy.ToURI();
      std::move(callback).Run();
      return;
    }
  }

  proxy_config_client_->MarkProxiesAsBad(bypass_duration, bad_proxies,
                                         std::move(callback));
}

void DataReductionProxyIOData::AddThrottleConfigObserver(
    mojom::DataReductionProxyThrottleConfigObserverPtr observer) {
  observer->OnThrottleConfigChanged(CreateThrottleConfig());
  drp_throttle_config_observers_.AddPtr(std::move(observer));
}

void DataReductionProxyIOData::AddThrottleConfigObserverInfo(
    mojom::DataReductionProxyThrottleConfigObserverPtrInfo observer) {
  AddThrottleConfigObserver(
      mojom::DataReductionProxyThrottleConfigObserverPtr(std::move(observer)));
}

void DataReductionProxyIOData::Clone(mojom::DataReductionProxyRequest request) {
  drp_bindings_.AddBinding(this, std::move(request));
}

}  // namespace data_reduction_proxy
