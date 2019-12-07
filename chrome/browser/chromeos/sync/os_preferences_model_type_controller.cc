// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/os_preferences_model_type_controller.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync/model_impl/syncable_service_based_bridge.h"

using syncer::ClientTagBasedModelTypeProcessor;
using syncer::ForwardingModelTypeControllerDelegate;
using syncer::SyncableServiceBasedBridge;

// static
std::unique_ptr<OsPreferencesModelTypeController>
OsPreferencesModelTypeController::Create(
    syncer::ModelType type,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    PrefService* pref_service,
    syncer::SyncService* sync_service) {
  // The bridge must be created first so that it can be used to construct the
  // delegates passed to the superclass constructor.
  auto bridge = std::make_unique<SyncableServiceBasedBridge>(
      type, std::move(store_factory),
      std::make_unique<ClientTagBasedModelTypeProcessor>(type, dump_stack),
      syncable_service.get());
  // Calls new because the constructor is private.
  return base::WrapUnique(new OsPreferencesModelTypeController(
      type, std::move(bridge), pref_service, sync_service));
}

OsPreferencesModelTypeController::OsPreferencesModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<syncer::ModelTypeSyncBridge> bridge,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : ModelTypeController(
          type,
          /*delegate_for_full_sync_mode=*/
          std::make_unique<ForwardingModelTypeControllerDelegate>(
              bridge->change_processor()->GetControllerDelegate().get()),
          /*delegate_for_transport_mode=*/
          std::make_unique<ForwardingModelTypeControllerDelegate>(
              bridge->change_processor()->GetControllerDelegate().get())),
      bridge_(std::move(bridge)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK(type == syncer::OS_PREFERENCES ||
         type == syncer::OS_PRIORITY_PREFERENCES);
  DCHECK(pref_service_);
  DCHECK(sync_service_);

  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      syncer::prefs::kOsSyncFeatureEnabled,
      base::BindRepeating(&OsPreferencesModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

OsPreferencesModelTypeController::~OsPreferencesModelTypeController() = default;

syncer::DataTypeController::PreconditionState
OsPreferencesModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void OsPreferencesModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
