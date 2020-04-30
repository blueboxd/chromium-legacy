// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/synced_network_metrics_logger.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace sync_wifi {

namespace {

const char kFailureReasonAllHistogram[] =
    "Network.Wifi.Synced.Connection.FailureReason";
const char kConnectionResultAllHistogram[] =
    "Network.Wifi.Synced.Connection.Result";

const char kFailureReasonManualHistogram[] =
    "Network.Wifi.Synced.ManualConnection.FailureReason";
const char kConnectionResultManualHistogram[] =
    "Network.Wifi.Synced.ManualConnection.Result";

}  // namespace

// static
SyncedNetworkMetricsLogger::ConnectionFailureReason
SyncedNetworkMetricsLogger::FailureReasonToEnum(const std::string& reason) {
  if (reason == shill::kErrorBadPassphrase)
    return ConnectionFailureReason::kBadPassphrase;
  else if (reason == shill::kErrorBadWEPKey)
    return ConnectionFailureReason::kBadWepKey;
  else if (reason == shill::kErrorConnectFailed)
    return ConnectionFailureReason::kFailedToConnect;
  else if (reason == shill::kErrorDhcpFailed)
    return ConnectionFailureReason::kDhcpFailure;
  else if (reason == shill::kErrorDNSLookupFailed)
    return ConnectionFailureReason::kDnsLookupFailure;
  else if (reason == shill::kErrorEapAuthenticationFailed)
    return ConnectionFailureReason::kEapAuthentication;
  else if (reason == shill::kErrorEapLocalTlsFailed)
    return ConnectionFailureReason::kEapLocalTls;
  else if (reason == shill::kErrorEapRemoteTlsFailed)
    return ConnectionFailureReason::kEapRemoteTls;
  else if (reason == shill::kErrorOutOfRange)
    return ConnectionFailureReason::kOutOfRange;
  else if (reason == shill::kErrorPinMissing)
    return ConnectionFailureReason::kPinMissing;
  else if (reason == shill::kErrorNoFailure)
    return ConnectionFailureReason::kNoFailure;
  else if (reason == shill::kErrorNotAssociated)
    return ConnectionFailureReason::kNotAssociated;
  else if (reason == shill::kErrorNotAuthenticated)
    return ConnectionFailureReason::kNotAuthenticated;
  else if (reason == shill::kErrorTooManySTAs)
    return ConnectionFailureReason::kTooManySTAs;
  else
    return ConnectionFailureReason::kUnknown;
}

SyncedNetworkMetricsLogger::SyncedNetworkMetricsLogger(
    NetworkStateHandler* network_state_handler,
    NetworkConnectionHandler* network_connection_handler) {
  if (network_state_handler) {
    network_state_handler_ = network_state_handler;
    network_state_handler_->AddObserver(this, FROM_HERE);
  }

  if (network_connection_handler) {
    network_connection_handler_ = network_connection_handler;
    network_connection_handler_->AddObserver(this);
  }
}

SyncedNetworkMetricsLogger::~SyncedNetworkMetricsLogger() {
  if (network_connection_handler_)
    network_connection_handler_->RemoveObserver(this);
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
}

void SyncedNetworkMetricsLogger::ConnectSucceeded(
    const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!IsEligible(network))
    return;

  UMA_HISTOGRAM_BOOLEAN(kConnectionResultManualHistogram, true);
}
void SyncedNetworkMetricsLogger::ConnectFailed(const std::string& service_path,
                                               const std::string& error_name) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);
  if (!IsEligible(network))
    return;

  // Get the the current state and error info.
  NetworkHandler::Get()->network_configuration_handler()->GetShillProperties(
      service_path,
      base::BindOnce(
          &SyncedNetworkMetricsLogger::ConnectErrorPropertiesSucceeded,
          weak_ptr_factory_.GetWeakPtr(), error_name),
      base::BindRepeating(
          &SyncedNetworkMetricsLogger::ConnectErrorPropertiesFailed,
          weak_ptr_factory_.GetWeakPtr(), error_name, service_path));
}

void SyncedNetworkMetricsLogger::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!IsEligible(network)) {
    return;
  }

  if (network->IsConnectingState()) {
    if (!connecting_guids_.contains(network->guid())) {
      connecting_guids_.insert(network->guid());
    }
    return;
  }

  if (!connecting_guids_.contains(network->guid())) {
    return;
  }

  if (network->connection_state() == shill::kStateFailure) {
    UMA_HISTOGRAM_BOOLEAN(kConnectionResultAllHistogram, false);
    UMA_HISTOGRAM_ENUMERATION(kFailureReasonAllHistogram,
                              FailureReasonToEnum(network->GetError()));
  } else if (network->IsConnectedState()) {
    UMA_HISTOGRAM_BOOLEAN(kConnectionResultAllHistogram, true);
  }
  connecting_guids_.erase(network->guid());
}

bool SyncedNetworkMetricsLogger::IsEligible(const NetworkState* network) {
  return network &&
         NetworkHandler::Get()->network_metadata_store()->GetIsConfiguredBySync(
             network->guid());
}

void SyncedNetworkMetricsLogger::ConnectErrorPropertiesSucceeded(
    const std::string& error_name,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  std::string state;
  shill_properties.GetStringWithoutPathExpansion(shill::kStateProperty, &state);
  if (NetworkState::StateIsConnected(state) ||
      NetworkState::StateIsConnecting(state)) {
    // If network is no longer in an error state, don't record it.
    return;
  }

  std::string shill_error;
  shill_properties.GetStringWithoutPathExpansion(shill::kErrorProperty,
                                                 &shill_error);
  if (!NetworkState::ErrorIsValid(shill_error)) {
    shill_properties.GetStringWithoutPathExpansion(
        shill::kPreviousErrorProperty, &shill_error);
    if (!NetworkState::ErrorIsValid(shill_error))
      shill_error = error_name;
  }
  UMA_HISTOGRAM_BOOLEAN(kConnectionResultManualHistogram, false);
  UMA_HISTOGRAM_ENUMERATION(kFailureReasonManualHistogram,
                            FailureReasonToEnum(shill_error));
}

void SyncedNetworkMetricsLogger::ConnectErrorPropertiesFailed(
    const std::string& error_name,
    const std::string& service_path,
    const std::string& request_error,
    std::unique_ptr<base::DictionaryValue> shill_error_data) {
  UMA_HISTOGRAM_BOOLEAN(kConnectionResultManualHistogram, false);
  UMA_HISTOGRAM_ENUMERATION(kFailureReasonManualHistogram,
                            FailureReasonToEnum(error_name));
}

}  // namespace sync_wifi

}  // namespace chromeos
