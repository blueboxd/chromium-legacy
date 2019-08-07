// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_wallet_data_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/sync_error.h"
#include "components/sync/model/syncable_service.h"

namespace browser_sync {

AutofillWalletDataTypeController::AutofillWalletDataTypeController(
    syncer::ModelType type,
    scoped_refptr<base::SequencedTaskRunner> db_thread,
    const base::RepeatingClosure& dump_stack,
    syncer::SyncService* sync_service,
    syncer::SyncClient* sync_client,
    const PersonalDataManagerProvider& pdm_provider,
    const scoped_refptr<autofill::AutofillWebDataService>& web_data_service)
    : AsyncDirectoryTypeController(type,
                                   dump_stack,
                                   sync_service,
                                   sync_client,
                                   syncer::GROUP_DB,
                                   std::move(db_thread)),
      pdm_provider_(pdm_provider),
      callback_registered_(false),
      web_data_service_(web_data_service),
      currently_enabled_(IsEnabled()) {
  DCHECK(type == syncer::AUTOFILL_WALLET_METADATA);
  pref_registrar_.Init(sync_client->GetPrefService());
  pref_registrar_.Add(
      autofill::prefs::kAutofillWalletImportEnabled,
      base::BindRepeating(&AutofillWalletDataTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
  pref_registrar_.Add(
      autofill::prefs::kAutofillCreditCardEnabled,
      base::BindRepeating(&AutofillWalletDataTypeController::OnUserPrefChanged,
                          base::AsWeakPtr(this)));
}

AutofillWalletDataTypeController::~AutofillWalletDataTypeController() {}

bool AutofillWalletDataTypeController::StartModels() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(state(), MODEL_STARTING);

  if (!IsEnabled())
    return false;

  if (!web_data_service_)
    return false;

  if (web_data_service_->IsDatabaseLoaded())
    return true;

  if (!callback_registered_) {
    web_data_service_->RegisterDBLoadedCallback(
        base::BindRepeating(&AutofillWalletDataTypeController::OnModelLoaded,
                            base::AsWeakPtr(this)));
    callback_registered_ = true;
  }

  return false;
}

syncer::DataTypeController::PreconditionState
AutofillWalletDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return currently_enabled_ ? PreconditionState::kPreconditionsMet
                            : PreconditionState::kMustStopAndClearData;
}

void AutofillWalletDataTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());

  bool new_enabled = IsEnabled();
  if (currently_enabled_ == new_enabled)
    return;  // No change to sync state.
  currently_enabled_ = new_enabled;

  sync_service()->DataTypePreconditionChanged(type());
}

bool AutofillWalletDataTypeController::IsEnabled() {
  DCHECK(CalledOnValidThread());

  // Require the user-visible pref to be enabled to sync Wallet data/metadata.
  return sync_client()->GetPrefService()->GetBoolean(
             autofill::prefs::kAutofillWalletImportEnabled) &&
         sync_client()->GetPrefService()->GetBoolean(
             autofill::prefs::kAutofillCreditCardEnabled);
}

}  // namespace browser_sync
