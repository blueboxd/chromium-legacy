// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/synced_network_updater_impl.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/values.h"
#include "chromeos/components/sync_wifi/network_type_conversions.h"
#include "chromeos/components/sync_wifi/timer_factory.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "components/device_event_log/device_event_log.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace sync_wifi {

namespace {

const int kMaxRetries = 3;
const char kTimedOutErrorMsg[] = "Timed out";
constexpr base::TimeDelta kTimeout = base::TimeDelta::FromMinutes(1);

}  // namespace

SyncedNetworkUpdaterImpl::SyncedNetworkUpdaterImpl(
    std::unique_ptr<PendingNetworkConfigurationTracker> tracker,
    network_config::mojom::CrosNetworkConfig* cros_network_config,
    std::unique_ptr<TimerFactory> timer_factory)
    : tracker_(std::move(tracker)),
      cros_network_config_(cros_network_config),
      timer_factory_(std::move(timer_factory)) {
  cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  // Load the current list of networks.
  OnNetworkStateListChanged();
  std::vector<PendingNetworkConfigurationUpdate> updates =
      tracker_->GetPendingUpdates();
  for (const PendingNetworkConfigurationUpdate& update : updates)
    Retry(update);
}

SyncedNetworkUpdaterImpl::~SyncedNetworkUpdaterImpl() = default;

void SyncedNetworkUpdaterImpl::AddOrUpdateNetwork(
    const sync_pb::WifiConfigurationSpecificsData& specifics) {
  auto id = NetworkIdentifier::FromProto(specifics);
  std::string change_guid = tracker_->TrackPendingUpdate(id, specifics);
  StartAddOrUpdateOperation(change_guid, id, specifics);
}

void SyncedNetworkUpdaterImpl::StartAddOrUpdateOperation(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    const sync_pb::WifiConfigurationSpecificsData& specifics) {
  network_config::mojom::NetworkStatePropertiesPtr existing_network =
      FindMojoNetwork(id);
  network_config::mojom::ConfigPropertiesPtr config =
      MojoNetworkConfigFromProto(specifics);

  StartTimer(change_guid, id);

  if (existing_network) {
    cros_network_config_->SetProperties(
        existing_network->guid, std::move(config),
        base::BindOnce(&SyncedNetworkUpdaterImpl::OnSetPropertiesResult,
                       weak_ptr_factory_.GetWeakPtr(), change_guid, id));
    return;
  }

  cros_network_config_->ConfigureNetwork(
      std::move(config), /*shared=*/false,
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnConfigureNetworkResult,
                     weak_ptr_factory_.GetWeakPtr(), change_guid, id));
}

void SyncedNetworkUpdaterImpl::RemoveNetwork(const NetworkIdentifier& id) {
  network_config::mojom::NetworkStatePropertiesPtr network =
      FindMojoNetwork(id);
  if (!network)
    return;

  std::string change_guid =
      tracker_->TrackPendingUpdate(id, /*specifics=*/base::nullopt);
  StartDeleteOperation(change_guid, id, network->guid);
}

void SyncedNetworkUpdaterImpl::StartDeleteOperation(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    std::string guid) {
  StartTimer(change_guid, id);
  cros_network_config_->ForgetNetwork(
      guid, base::BindOnce(&SyncedNetworkUpdaterImpl::OnForgetNetworkResult,
                           weak_ptr_factory_.GetWeakPtr(), change_guid, id));
}

network_config::mojom::NetworkStatePropertiesPtr
SyncedNetworkUpdaterImpl::FindMojoNetwork(const NetworkIdentifier& id) {
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       networks_) {
    if (id == NetworkIdentifier::FromMojoNetwork(network))
      return network.Clone();
  }
  return nullptr;
}

void SyncedNetworkUpdaterImpl::OnNetworkStateListChanged() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kConfigured,
          network_config::mojom::NetworkType::kWiFi,
          /*limit=*/0),
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnGetNetworkList,
                     base::Unretained(this)));
}

void SyncedNetworkUpdaterImpl::OnGetNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  networks_ = std::move(networks);
}

void SyncedNetworkUpdaterImpl::OnError(const std::string& change_guid,
                                       const NetworkIdentifier& id,
                                       const std::string& error_name) {
  NET_LOG(ERROR) << "Failed to update id:" << id.SerializeToString()
                 << " error:" << error_name;
  HandleShillResult(change_guid, id, /*is_success=*/false);
}

void SyncedNetworkUpdaterImpl::OnConfigureNetworkResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    const base::Optional<std::string>& guid,
    const std::string& error_message) {
  if (guid) {
    VLOG(1) << "Successfully configured network with id "
            << id.SerializeToString();
  } else {
    NET_LOG(ERROR) << "Failed to configure network with id "
                   << id.SerializeToString() << ". " << error_message;
  }
  HandleShillResult(change_guid, id, guid.has_value());
}

void SyncedNetworkUpdaterImpl::OnSetPropertiesResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    bool is_success,
    const std::string& error_message) {
  if (is_success) {
    VLOG(1) << "Successfully updated network with id "
            << id.SerializeToString();
  } else {
    NET_LOG(ERROR) << "Failed to update network with id "
                   << id.SerializeToString();
  }
  HandleShillResult(change_guid, id, is_success);
}

void SyncedNetworkUpdaterImpl::OnForgetNetworkResult(
    const std::string& change_guid,
    const NetworkIdentifier& id,
    bool is_success) {
  if (is_success)
    VLOG(1) << "Successfully deleted network with id "
            << id.SerializeToString();
  else
    NET_LOG(ERROR) << "Failed to remove network with id "
                   << id.SerializeToString();

  HandleShillResult(change_guid, id, is_success);
}

void SyncedNetworkUpdaterImpl::HandleShillResult(const std::string& change_guid,
                                                 const NetworkIdentifier& id,
                                                 bool is_success) {
  change_guid_to_timer_map_.erase(change_guid);
  if (is_success) {
    tracker_->MarkComplete(change_guid, id);
    return;
  }

  if (!tracker_->GetPendingUpdate(change_guid, id)) {
    VLOG(1) << "Update to network " << id.SerializeToString()
            << " with change_guid " << change_guid
            << " is no longer pending.  This is usually because it was "
               "preempted by another update to the same network.";
    return;
  }
  tracker_->IncrementCompletedAttempts(change_guid, id);

  base::Optional<PendingNetworkConfigurationUpdate> update =
      tracker_->GetPendingUpdate(change_guid, id);
  if (update->completed_attempts() >= kMaxRetries) {
    LOG(ERROR) << "Ran out of retries updating network with id "
               << id.SerializeToString();
    tracker_->MarkComplete(change_guid, id);
    return;
  }

  Retry(*update);
}

void SyncedNetworkUpdaterImpl::CleanupUpdate(const std::string& change_guid,
                                             const NetworkIdentifier& id) {
  tracker_->MarkComplete(change_guid, id);
}

void SyncedNetworkUpdaterImpl::Retry(
    const PendingNetworkConfigurationUpdate& update) {
  if (update.IsDeleteOperation()) {
    network_config::mojom::NetworkStatePropertiesPtr network =
        FindMojoNetwork(update.id());
    if (!network) {
      tracker_->MarkComplete(update.change_guid(), update.id());
      return;
    }

    StartDeleteOperation(update.change_guid(), update.id(), network->guid);
    return;
  }

  StartAddOrUpdateOperation(update.change_guid(), update.id(),
                            update.specifics().value());
}

void SyncedNetworkUpdaterImpl::StartTimer(const std::string& change_guid,
                                          const NetworkIdentifier& id) {
  change_guid_to_timer_map_[change_guid] = timer_factory_->CreateOneShotTimer();
  change_guid_to_timer_map_[change_guid]->Start(
      FROM_HERE, kTimeout,
      base::BindOnce(&SyncedNetworkUpdaterImpl::OnError, base::Unretained(this),
                     change_guid, id, kTimedOutErrorMsg));
}

}  // namespace sync_wifi

}  // namespace chromeos
