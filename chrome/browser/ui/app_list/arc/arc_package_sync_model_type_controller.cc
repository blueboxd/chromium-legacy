// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_package_sync_model_type_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

using syncer::ClientTagBasedModelTypeProcessor;
using syncer::ForwardingModelTypeControllerDelegate;
using syncer::ModelTypeController;
using syncer::ModelTypeControllerDelegate;
using syncer::SyncableServiceBasedBridge;

// static
std::unique_ptr<ArcPackageSyncModelTypeController>
ArcPackageSyncModelTypeController::Create(
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    syncer::SyncService* sync_service,
    Profile* profile) {
  auto bridge = std::make_unique<SyncableServiceBasedBridge>(
      syncer::ARC_PACKAGE, std::move(store_factory),
      std::make_unique<ClientTagBasedModelTypeProcessor>(syncer::ARC_PACKAGE,
                                                         dump_stack),
      syncable_service.get());
  ModelTypeControllerDelegate* delegate =
      bridge->change_processor()->GetControllerDelegate().get();
  auto delegate_for_full_sync_mode =
      std::make_unique<ForwardingModelTypeControllerDelegate>(delegate);

  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    // Runs in transport-mode and full-sync mode, sharing the bridge's delegate.
    return base::WrapUnique(new ArcPackageSyncModelTypeController(
        std::move(bridge), std::move(delegate_for_full_sync_mode),
        /*delegate_for_transport_mode=*/
        std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
        sync_service, profile));
  } else {
    // Only runs in full-sync mode.
    return base::WrapUnique(new ArcPackageSyncModelTypeController(
        std::move(bridge), std::move(delegate_for_full_sync_mode),
        /*delegate_for_transport_mode=*/
        nullptr, sync_service, profile));
  }
}

ArcPackageSyncModelTypeController::ArcPackageSyncModelTypeController(
    std::unique_ptr<syncer::ModelTypeSyncBridge> bridge,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode,
    syncer::SyncService* sync_service,
    Profile* profile)
    : ModelTypeController(syncer::ARC_PACKAGE,
                          std::move(delegate_for_full_sync_mode),
                          std::move(delegate_for_transport_mode)),
      bridge_(std::move(bridge)),
      sync_service_(sync_service),
      profile_(profile),
      arc_prefs_(ArcAppListPrefs::Get(profile)) {
  DCHECK(arc_prefs_);
  DCHECK(profile_);

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->AddObserver(this);
  }

  arc_prefs_->AddObserver(this);

  // See GetPreconditionState().
  if (chromeos::features::IsSplitSettingsSyncEnabled()) {
    pref_registrar_.Init(profile_->GetPrefs());
    pref_registrar_.Add(
        syncer::prefs::kOsSyncFeatureEnabled,
        base::BindRepeating(
            &ArcPackageSyncModelTypeController::OnOsSyncFeaturePrefChanged,
            base::Unretained(this)));
  }
}

ArcPackageSyncModelTypeController::~ArcPackageSyncModelTypeController() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->RemoveObserver(this);
  }
  arc_prefs_->RemoveObserver(this);
}

syncer::DataTypeController::PreconditionState
ArcPackageSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  if (!arc::IsArcPlayStoreEnabledForProfile(profile_)) {
    return PreconditionState::kMustStopAndClearData;
  }
  // Use OS sync feature consent for this ModelType because it can sync in
  // transport-only mode (and hence isn't tied to browser sync consent).
  if (chromeos::features::IsSplitSettingsSyncEnabled() &&
      !profile_->GetPrefs()->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)) {
    return PreconditionState::kMustStopAndClearData;
  }
  // Implementing a wait here in the controller, instead of the regular wait in
  // the SyncableService, allows waiting again after this particular datatype
  // has been disabled and reenabled (since core sync code does not support the
  // notion of a model becoming unready, which effectively is the case here).
  if (!arc_prefs_->package_list_initial_refreshed()) {
    return PreconditionState::kMustStopAndKeepData;
  }
  return PreconditionState::kPreconditionsMet;
}

void ArcPackageSyncModelTypeController::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ArcPackageSyncModelTypeController::OnArcInitialStart() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ArcPackageSyncModelTypeController::OnPackageListInitialRefreshed() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ArcPackageSyncModelTypeController::OnOsSyncFeaturePrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
