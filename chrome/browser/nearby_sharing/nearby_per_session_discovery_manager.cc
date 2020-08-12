// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_per_session_discovery_manager.h"

#include <string>

#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/nearby_confirmation_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

NearbyPerSessionDiscoveryManager::NearbyPerSessionDiscoveryManager(
    NearbySharingService* nearby_sharing_service)
    : nearby_sharing_service_(nearby_sharing_service) {}

NearbyPerSessionDiscoveryManager::~NearbyPerSessionDiscoveryManager() {
  UnregisterSendSurface();
}

void NearbyPerSessionDiscoveryManager::OnTransferUpdate(
    const ShareTarget& share_target,
    const TransferMetadata& transfer_metadata) {
  switch (transfer_metadata.status()) {
    case TransferMetadata::Status::kAwaitingLocalConfirmation: {
      DCHECK(select_share_target_callback_);
      mojo::PendingRemote<nearby_share::mojom::ConfirmationManager> remote;
      mojo::MakeSelfOwnedReceiver(std::make_unique<NearbyConfirmationManager>(
                                      nearby_sharing_service_, share_target),
                                  remote.InitWithNewPipeAndPassReceiver());
      std::move(select_share_target_callback_)
          .Run(nearby_share::mojom::SelectShareTargetResult::kOk,
               transfer_metadata.token(), std::move(remote));
      break;
    }
    case TransferMetadata::Status::kAwaitingRemoteAcceptance:
      DCHECK(select_share_target_callback_);
      std::move(select_share_target_callback_)
          .Run(nearby_share::mojom::SelectShareTargetResult::kOk,
               /*token=*/base::nullopt, mojo::NullRemote());
      break;
    default:
      if (transfer_metadata.is_final_status() &&
          select_share_target_callback_) {
        std::move(select_share_target_callback_)
            .Run(nearby_share::mojom::SelectShareTargetResult::kError,
                 /*token=*/base::nullopt, mojo::NullRemote());
      }
      break;
  }
}

void NearbyPerSessionDiscoveryManager::OnShareTargetDiscovered(
    ShareTarget share_target) {
  base::InsertOrAssign(discovered_share_targets_, share_target.id,
                       share_target);
  share_target_listener_->OnShareTargetDiscovered(share_target);
}

void NearbyPerSessionDiscoveryManager::OnShareTargetLost(
    ShareTarget share_target) {
  discovered_share_targets_.erase(share_target.id);
  share_target_listener_->OnShareTargetLost(share_target);
}

void NearbyPerSessionDiscoveryManager::StartDiscovery(
    mojo::PendingRemote<nearby_share::mojom::ShareTargetListener> listener,
    StartDiscoveryCallback callback) {
  share_target_listener_.Bind(std::move(listener));
  // |share_target_listener_| owns the callbacks and is guaranteed to be
  // destroyed before |this|, therefore making base::Unretained() safe to use.
  share_target_listener_.set_disconnect_handler(
      base::BindOnce(&NearbyPerSessionDiscoveryManager::UnregisterSendSurface,
                     base::Unretained(this)));

  if (nearby_sharing_service_->RegisterSendSurface(
          this, this, NearbySharingService::SendSurfaceState::kForeground) !=
      NearbySharingService::StatusCodes::kOk) {
    NS_LOG(WARNING) << "Failed to register send surface";
    share_target_listener_.reset();
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::move(callback).Run(/*success=*/true);
}

void NearbyPerSessionDiscoveryManager::SelectShareTarget(
    const base::UnguessableToken& share_target_id,
    SelectShareTargetCallback callback) {
  DCHECK(share_target_listener_.is_bound());
  DCHECK(!select_share_target_callback_);

  auto iter = discovered_share_targets_.find(share_target_id);
  if (iter == discovered_share_targets_.end()) {
    NS_LOG(VERBOSE) << "Unknown share target selected: id=" << share_target_id;
    std::move(callback).Run(
        nearby_share::mojom::SelectShareTargetResult::kInvalidShareTarget,
        /*token=*/base::nullopt, mojo::NullRemote());
    return;
  }

  select_share_target_callback_ = std::move(callback);
  // TODO(crbug.com/1099710): Call correct method and pass attachments.
  nearby_sharing_service_->SendText(
      iter->second, "Example Text",
      base::BindOnce(&NearbyPerSessionDiscoveryManager::OnSend,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NearbyPerSessionDiscoveryManager::OnSend(
    NearbySharingService::StatusCodes status) {
  // Nothing to do if the result has been returned already.
  if (!select_share_target_callback_)
    return;

  // If the send call succeeded, we expect OnTransferUpdate() to be called next.
  if (status == NearbySharingService::StatusCodes::kOk)
    return;

  NS_LOG(VERBOSE) << "Failed to select share target";
  std::move(select_share_target_callback_)
      .Run(nearby_share::mojom::SelectShareTargetResult::kError,
           /*token=*/base::nullopt, mojo::NullRemote());
}

void NearbyPerSessionDiscoveryManager::UnregisterSendSurface() {
  if (!share_target_listener_.is_bound())
    return;

  share_target_listener_.reset();

  if (nearby_sharing_service_->UnregisterSendSurface(this, this) !=
      NearbySharingService::StatusCodes::kOk) {
    NS_LOG(WARNING) << "Failed to unregister send surface";
  }
}
