// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "net/base/network_change_notifier_posix.h"
#include "net/dns/dns_config_service_posix.h"

#if defined(OS_ANDROID)
#include "net/android/network_change_notifier_android.h"
#endif

namespace net {

NetworkChangeNotifierPosix::NetworkChangeNotifierPosix(
    NetworkChangeNotifier::ConnectionType initial_connection_type,
    NetworkChangeNotifier::ConnectionSubtype initial_connection_subtype)
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsPosix()),
      dns_config_service_runner_(
          base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()})),
      dns_config_service_(
          nullptr,
          // Ensure DnsConfigService lives on |dns_config_service_runner_|
          // to prevent races where NetworkChangeNotifierPosix outlives
          // ScopedTaskEnvironment. https://crbug.com/938126
          base::OnTaskRunnerDeleter(dns_config_service_runner_)),
      connection_type_(initial_connection_type),
      max_bandwidth_mbps_(
          NetworkChangeNotifier::GetMaxBandwidthMbpsForConnectionSubtype(
              initial_connection_subtype)) {
  SetAndStartDnsConfigService(DnsConfigService::CreateSystemService());
  OnDNSChanged();
}

NetworkChangeNotifierPosix::~NetworkChangeNotifierPosix() {
  ClearGlobalPointer();
}

void NetworkChangeNotifierPosix::OnDNSChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  dns_config_service_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DnsConfigService::RefreshConfig,
                                base::Unretained(dns_config_service_.get())));
}

void NetworkChangeNotifierPosix::OnIPAddressChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
}

void NetworkChangeNotifierPosix::OnConnectionChanged(
    NetworkChangeNotifier::ConnectionType connection_type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    base::AutoLock scoped_lock(lock_);
    connection_type_ = connection_type;
  }
  NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
}

void NetworkChangeNotifierPosix::OnConnectionSubtypeChanged(
    NetworkChangeNotifier::ConnectionType connection_type,
    NetworkChangeNotifier::ConnectionSubtype connection_subtype) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  double max_bandwidth_mbps =
      GetMaxBandwidthMbpsForConnectionSubtype(connection_subtype);
  {
    base::AutoLock scoped_lock(lock_);
    max_bandwidth_mbps_ = max_bandwidth_mbps;
  }
  NetworkChangeNotifier::NotifyObserversOfMaxBandwidthChange(max_bandwidth_mbps,
                                                             connection_type);
}

void NetworkChangeNotifierPosix::SetDnsConfigServiceForTesting(
    std::unique_ptr<DnsConfigService> dns_config_service) {
  SetAndStartDnsConfigService(std::move(dns_config_service));
}

NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierPosix::GetCurrentConnectionType() const {
  base::AutoLock scoped_lock(lock_);
  return connection_type_;
}

void NetworkChangeNotifierPosix::GetCurrentMaxBandwidthAndConnectionType(
    double* max_bandwidth_mbps,
    ConnectionType* connection_type) const {
  base::AutoLock scoped_lock(lock_);
  *connection_type = connection_type_;
  *max_bandwidth_mbps = max_bandwidth_mbps_;
}

void NetworkChangeNotifierPosix::SetAndStartDnsConfigService(
    std::unique_ptr<DnsConfigService> dns_config_service) {
  // Reset/release to use the deleter already set up in |dns_config_service_|.
  dns_config_service_.reset(dns_config_service.release());
  dns_config_service_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DnsConfigService::WatchConfig,
                                base::Unretained(dns_config_service_.get()),
                                base::BindRepeating(
                                    &NetworkChangeNotifier::SetDnsConfig)));
}

// static
NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierPosix::NetworkChangeCalculatorParamsPosix() {
  NetworkChangeCalculatorParams params;
#if defined(OS_CHROMEOS)
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::TimeDelta::FromMilliseconds(4000);
  params.ip_address_online_delay_ = base::TimeDelta::FromMilliseconds(1000);
  params.connection_type_offline_delay_ =
      base::TimeDelta::FromMilliseconds(500);
  params.connection_type_online_delay_ = base::TimeDelta::FromMilliseconds(500);
#elif defined(OS_ANDROID)
  params =
      net::NetworkChangeNotifierAndroid::NetworkChangeCalculatorParamsAndroid();
#else
  NOTIMPLEMENTED();
#endif
  return params;
}

}  // namespace net
