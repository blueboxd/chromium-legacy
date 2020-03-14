// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/proxy_server.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/configured_proxy_resolution_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

using base::FieldTrialList;

namespace {

#if defined(OS_CHROMEOS)
// SequencedTaskRunner to get the network id. A SequencedTaskRunner is used
// rather than parallel tasks to avoid having many threads getting the network
// id concurrently.
base::LazyThreadPoolSequencedTaskRunner g_get_network_id_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(),
                         base::TaskPriority::BEST_EFFORT,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
#endif

// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  // The client IP address changed.
  DEPRECATED_IP_CHANGED = 0,
  // [Deprecated] Proxy is disabled because a VPN is running.
  DEPRECATED_DISABLED_ON_VPN = 1,
  // There was a network change.
  NETWORK_CHANGED = 2,
  CHANGE_EVENT_COUNT = NETWORK_CHANGED + 1

};


// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents", event,
                            CHANGE_EVENT_COUNT);
}


// Returns the current connection type if known, otherwise returns
// CONNECTION_UNKNOWN.
network::mojom::ConnectionType GetConnectionType(
    network::NetworkConnectionTracker* network_connection_tracker) {
  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network_connection_tracker->GetConnectionType(&connection_type,
                                                base::DoNothing());
  return connection_type;
}

std::string DoGetCurrentNetworkID(
    network::NetworkConnectionTracker* network_connection_tracker) {
  // It is possible that the connection type changed between when
  // GetConnectionType() was called and when the API to determine the
  // network name was called. Check if that happened and retry until the
  // connection type stabilizes. This is an imperfect solution but should
  // capture majority of cases, and should not significantly affect estimates
  // (that are approximate to begin with).

  while (true) {
    auto connection_type = GetConnectionType(network_connection_tracker);
    std::string ssid_mccmnc;

    switch (connection_type) {
      case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
      case network::mojom::ConnectionType::CONNECTION_NONE:
      case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      case network::mojom::ConnectionType::CONNECTION_ETHERNET:
        break;
      case network::mojom::ConnectionType::CONNECTION_WIFI:
// Get WiFi SSID only on Android since calling it on non-Android
// platforms may result in hung IO loop. See https://crbg.com/896296.
#if defined(OS_ANDROID)
        ssid_mccmnc = net::GetWifiSSID();
#endif
        break;
      case network::mojom::ConnectionType::CONNECTION_2G:
      case network::mojom::ConnectionType::CONNECTION_3G:
      case network::mojom::ConnectionType::CONNECTION_4G:
#if defined(OS_ANDROID)
        ssid_mccmnc = net::android::GetTelephonyNetworkOperator();
#endif
        break;
    }

    if (connection_type == GetConnectionType(network_connection_tracker)) {
      if (connection_type >= network::mojom::ConnectionType::CONNECTION_2G &&
          connection_type <= network::mojom::ConnectionType::CONNECTION_4G) {
        // No need to differentiate cellular connections by the exact
        // connection type.
        return "cell," + ssid_mccmnc;
      }
      return base::NumberToString(static_cast<int>(connection_type)) + "," +
             ssid_mccmnc;
    }
  }
  NOTREACHED();
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyConfig::DataReductionProxyConfig(
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<DataReductionProxyConfigValues> config_values)
    : unreachable_(false),
      enabled_by_user_(false),
      config_values_(std::move(config_values)),
      network_connection_tracker_(network_connection_tracker),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN),
      network_properties_manager_(nullptr) {
  DCHECK(network_connection_tracker_);

  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void DataReductionProxyConfig::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    NetworkPropertiesManager* manager,
    const std::string& user_agent) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_properties_manager_ = manager;

  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&DataReductionProxyConfig::OnConnectionChanged,
                     weak_factory_.GetWeakPtr()));
}

void DataReductionProxyConfig::OnNewClientConfigFetched() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReloadConfig();
}

void DataReductionProxyConfig::ReloadConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

base::Optional<DataReductionProxyTypeInfo>
DataReductionProxyConfig::FindConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_values_->FindConfiguredDataReductionProxy(proxy_server);
}

net::ProxyList DataReductionProxyConfig::GetAllConfiguredProxies() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_values_->GetAllConfiguredProxies();
}

bool DataReductionProxyConfig::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyConfig::ProxyRules& proxy_rules,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  // Data reduction proxy config is Type::PROXY_LIST_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME)
    return false;

  if (is_https)
    return false;

  const net::ProxyList* proxies =
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);

  if (!proxies)
    return false;

  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool bypassed = false;

  for (const net::ProxyServer& proxy : proxies->GetAll()) {
    if (!proxy.is_valid() || proxy.is_direct())
      continue;

    base::TimeDelta delay;
    if (FindConfiguredDataReductionProxy(proxy)) {
      if (!IsProxyBypassed(retry_map, proxy, &delay))
        return false;
      if (delay < min_delay)
        min_delay = delay;
      bypassed = true;
    }
  }

  if (min_retry_delay && bypassed)
    *min_retry_delay = min_delay;

  return bypassed;
}

bool DataReductionProxyConfig::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return IsProxyBypassedAtTime(retry_map, proxy_server, GetTicksNow(),
                               retry_delay);
}

bool DataReductionProxyConfig::ContainsDataReductionProxy(
    const net::ProxyConfig::ProxyRules& proxy_rules) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Data Reduction Proxy configurations are always Type::PROXY_LIST_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME)
    return false;

  const net::ProxyList* http_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("http");
  if (http_proxy_list && !http_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      FindConfiguredDataReductionProxy(http_proxy_list->Get())) {
    return true;
  }

  return false;
}

void DataReductionProxyConfig::SetProxyConfig(bool enabled, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_by_user_ = enabled;
  network_properties_manager_->OnChangeInNetworkID(GetCurrentNetworkID());

  ReloadConfig();

  if (enabled_by_user_) {
    HandleCaptivePortal();
  }
}

void DataReductionProxyConfig::HandleCaptivePortal() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool is_captive_portal = GetIsCaptivePortal();
  if (is_captive_portal == network_properties_manager_->IsCaptivePortal())
    return;
  network_properties_manager_->SetIsCaptivePortal(is_captive_portal);
  ReloadConfig();
}

bool DataReductionProxyConfig::GetIsCaptivePortal() const {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_ANDROID)
  return net::android::GetIsCaptivePortal();
#endif  // OS_ANDROID
  return false;
}

void DataReductionProxyConfig::UpdateConfigForTesting(
    bool enabled,
    bool secure_proxies_allowed,
    bool insecure_proxies_allowed) {
  enabled_by_user_ = enabled;
}

void DataReductionProxyConfig::SetNetworkPropertiesManagerForTesting(
    NetworkPropertiesManager* manager) {
  network_properties_manager_ = manager;
}


void DataReductionProxyConfig::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  connection_type_ = type;
  RecordNetworkChangeEvent(NETWORK_CHANGED);

#if defined(OS_CHROMEOS)
  if (get_network_id_asynchronously_) {
    base::PostTaskAndReplyWithResult(
        g_get_network_id_task_runner.Get().get(), FROM_HERE,
        base::BindOnce(&DoGetCurrentNetworkID,
                       base::Unretained(network_connection_tracker_)),
        base::BindOnce(&DataReductionProxyConfig::ContinueNetworkChanged,
                       weak_factory_.GetWeakPtr()));
    return;
  }
#endif  // defined(OS_CHROMEOS)

  ContinueNetworkChanged(GetCurrentNetworkID());
}

void DataReductionProxyConfig::ContinueNetworkChanged(
    const std::string& network_id) {
  network_properties_manager_->OnChangeInNetworkID(network_id);

  ReloadConfig();

  if (enabled_by_user_) {
    HandleCaptivePortal();
  }
}


void DataReductionProxyConfig::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt) {
  DCHECK(thread_checker_.CalledOnValidThread());
  http_rtt_ = http_rtt;
}

base::Optional<base::TimeDelta> DataReductionProxyConfig::GetHttpRttEstimate()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return http_rtt_;
}

bool DataReductionProxyConfig::enabled_by_user_and_reachable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return enabled_by_user_ && !unreachable_;
}

base::TimeTicks DataReductionProxyConfig::GetTicksNow() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::TimeTicks::Now();
}

std::vector<DataReductionProxyServer>
DataReductionProxyConfig::GetProxiesForHttp() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enabled_by_user_)
    return std::vector<DataReductionProxyServer>();

  return config_values_->proxies_for_http();
}

std::string DataReductionProxyConfig::GetCurrentNetworkID() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return DoGetCurrentNetworkID(network_connection_tracker_);
}

const NetworkPropertiesManager&
DataReductionProxyConfig::GetNetworkPropertiesManager() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return *network_properties_manager_;
}



#if defined(OS_CHROMEOS)
void DataReductionProxyConfig::EnableGetNetworkIdAsynchronously() {
  get_network_id_asynchronously_ = true;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace data_reduction_proxy
