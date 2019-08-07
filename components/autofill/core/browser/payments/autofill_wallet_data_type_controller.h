// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/async_directory_type_controller.h"

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}  // namespace autofill

namespace syncer {
class SyncClient;
class SyncService;
}  // namespace syncer

namespace browser_sync {

// Controls syncing of AUTOFILL_WALLET_METADATA.
class AutofillWalletDataTypeController
    : public syncer::AsyncDirectoryTypeController {
 public:
  using PersonalDataManagerProvider =
      base::RepeatingCallback<autofill::PersonalDataManager*()>;

  // |type| should be AUTOFILL_WALLET_METADATA.
  // |dump_stack| is called when an unrecoverable error occurs.
  AutofillWalletDataTypeController(
      syncer::ModelType type,
      scoped_refptr<base::SequencedTaskRunner> db_thread,
      const base::RepeatingClosure& dump_stack,
      syncer::SyncService* sync_service,
      syncer::SyncClient* sync_client,
      const PersonalDataManagerProvider& pdm_provider,
      const scoped_refptr<autofill::AutofillWebDataService>& web_data_service);
  ~AutofillWalletDataTypeController() override;

  // AsyncDirectoryTypeController implementation.
  bool StartModels() override;
  PreconditionState GetPreconditionState() const override;

 private:
  // Callback for changes to the autofill pref.
  void OnUserPrefChanged();

  // Returns true if the prefs are set such that wallet sync should be enabled.
  bool IsEnabled();

  // Callback that allows accessing PersonalDataManager lazily.
  const PersonalDataManagerProvider pdm_provider_;

  // Whether the database loaded callback has been registered.
  bool callback_registered_;

  // A reference to the AutofillWebDataService for this controller.
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;

  // Stores whether we're currently syncing wallet data. This is the last
  // value computed by IsEnabled.
  bool currently_enabled_;

  // Registrar for listening to kAutofillWalletSyncExperimentEnabled status.
  PrefChangeRegistrar pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletDataTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_AUTOFILL_WALLET_DATA_TYPE_CONTROLLER_H_
