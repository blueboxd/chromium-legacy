// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_METRICS_LOGGER_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_METRICS_LOGGER_H_

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {

class NetworkState;
class NetworkStateHandler;
class NetworkConnectionHandler;

namespace sync_wifi {

// Logs connection metrics for networks which were configured by sync.
class SyncedNetworkMetricsLogger : public NetworkConnectionObserver,
                                   public NetworkStateHandlerObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ConnectionFailureReason {
    kUnknownDeprecated = 0,  // deprecated
    kBadPassphrase = 1,
    kBadWepKey = 2,
    kFailedToConnect = 3,
    kDhcpFailure = 4,
    kDnsLookupFailure = 5,
    kEapAuthentication = 6,
    kEapLocalTls = 7,
    kEapRemoteTls = 8,
    kOutOfRange = 9,
    kPinMissing = 10,
    kUnknown = 11,
    kNoFailure = 12,
    kNotAssociated = 13,
    kNotAuthenticated = 14,
    kTooManySTAs = 15,
    kMaxValue = kTooManySTAs
  };

  SyncedNetworkMetricsLogger(
      NetworkStateHandler* network_state_handler,
      NetworkConnectionHandler* network_connection_handler);
  ~SyncedNetworkMetricsLogger() override;

  SyncedNetworkMetricsLogger(const SyncedNetworkMetricsLogger&) = delete;
  SyncedNetworkMetricsLogger& operator=(const SyncedNetworkMetricsLogger&) =
      delete;

  // NetworkConnectionObserver::
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;

  // NetworkStateObserver::
  void NetworkConnectionStateChanged(const NetworkState* network) override;

 private:
  static ConnectionFailureReason FailureReasonToEnum(const std::string& reason);

  void ConnectErrorPropertiesSucceeded(
      const std::string& error_name,
      const std::string& service_path,
      const base::DictionaryValue& shill_properties);
  void ConnectErrorPropertiesFailed(
      const std::string& error_name,
      const std::string& service_path,
      const std::string& request_error,
      std::unique_ptr<base::DictionaryValue> shill_error_data);

  bool IsEligible(const NetworkState* network);

  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkConnectionHandler* network_connection_handler_ = nullptr;

  // Contains the guids of networks which are currently connecting.
  base::flat_set<std::string> connecting_guids_;

  base::WeakPtrFactory<SyncedNetworkMetricsLogger> weak_ptr_factory_{this};
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_SYNCED_NETWORK_METRICS_LOGGER_H_
