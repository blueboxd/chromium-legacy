// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_package_sync_model_type_controller.h"

#include <utility>

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/driver/sync_service.h"

ArcPackageSyncModelTypeController::ArcPackageSyncModelTypeController(
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    syncer::SyncService* sync_service,
    Profile* profile)
    : syncer::SyncableServiceBasedModelTypeController(syncer::ARC_PACKAGE,
                                                      std::move(store_factory),
                                                      syncable_service,
                                                      dump_stack),
      sync_service_(sync_service),
      profile_(profile) {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->AddObserver(this);
  }
}

ArcPackageSyncModelTypeController::~ArcPackageSyncModelTypeController() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->RemoveObserver(this);
  }
}

syncer::DataTypeController::PreconditionState
ArcPackageSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return arc::IsArcPlayStoreEnabledForProfile(profile_)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
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
