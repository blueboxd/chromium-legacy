// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_wallet_data_type_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/configure_context.h"
#include "components/sync/driver/data_type_controller_mock.h"
#include "components/sync/driver/fake_generic_change_processor.h"
#include "components/sync/driver/mock_sync_service.h"
#include "components/sync/driver/sync_client_mock.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/fake_syncable_service.h"
#include "components/sync/model/sync_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using autofill::AutofillWebDataService;
using testing::_;
using testing::Return;

// Fake WebDataService implementation that stubs out the database loading.
class FakeWebDataService : public AutofillWebDataService {
 public:
  FakeWebDataService(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& db_task_runner)
      : AutofillWebDataService(ui_task_runner, db_task_runner),
        is_database_loaded_(false),
        db_loaded_callback_(base::RepeatingCallback<void(void)>()) {}

  // Mark the database as loaded and send out the appropriate notification.
  void LoadDatabase() {
    is_database_loaded_ = true;

    if (!db_loaded_callback_.is_null()) {
      db_loaded_callback_.Run();
      // Clear the callback here or the WDS and DTC will have refs to each other
      // and create a memory leak.
      // TODO(crbug.com/941530): Solve this with a OnceCallback. Note that
      //     RegisterDBLoadedCallback overrides other functions that still use
      //     base::[Repeating]Callbacks, so it would affect non-Autofill code.
      db_loaded_callback_ = base::RepeatingCallback<void(void)>();
    }
  }

  bool IsDatabaseLoaded() override { return is_database_loaded_; }

  void RegisterDBLoadedCallback(
      const base::RepeatingCallback<void(void)>& callback) override {
    db_loaded_callback_ = callback;
  }

 private:
  ~FakeWebDataService() override {}

  bool is_database_loaded_;
  base::RepeatingCallback<void(void)> db_loaded_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebDataService);
};

class AutofillWalletDataTypeControllerTest : public testing::Test {
 public:
  AutofillWalletDataTypeControllerTest() : last_type_(syncer::UNSPECIFIED) {
    ON_CALL(sync_service_, GetUserShare()).WillByDefault(Return(&user_share_));
  }

  ~AutofillWalletDataTypeControllerTest() override {}

  void SetUp() override {
    prefs_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillWalletImportEnabled, true);
    prefs_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillCreditCardEnabled, true);

    ON_CALL(sync_client_, GetPrefService()).WillByDefault(Return(&prefs_));
    ON_CALL(sync_client_, GetSyncableServiceForType(_))
        .WillByDefault(Return(syncable_service_.AsWeakPtr()));

    web_data_service_ = base::MakeRefCounted<FakeWebDataService>(
        base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get());
    autofill_wallet_dtc_ = std::make_unique<AutofillWalletDataTypeController>(
        syncer::AUTOFILL_WALLET_METADATA, base::ThreadTaskRunnerHandle::Get(),
        /*dump_stack=*/base::DoNothing(), &sync_service_, &sync_client_,
        AutofillWalletDataTypeController::PersonalDataManagerProvider(),
        web_data_service_);

    last_type_ = syncer::UNSPECIFIED;
    last_error_ = syncer::SyncError();
  }

  void TearDown() override {
    // Make sure WebDataService is shutdown properly on DB thread before we
    // destroy it.
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(syncer::AUTOFILL_WALLET_METADATA);
  }

 protected:
  void SetStartExpectations() {
    autofill_wallet_dtc_->SetGenericChangeProcessorFactoryForTest(
        std::make_unique<syncer::FakeGenericChangeProcessorFactory>(
            std::make_unique<syncer::FakeGenericChangeProcessor>(
                syncer::AUTOFILL_WALLET_METADATA)));
  }

  bool Start() {
    autofill_wallet_dtc_->LoadModels(
        syncer::ConfigureContext(),
        base::BindRepeating(
            &AutofillWalletDataTypeControllerTest::OnLoadFinished,
            base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    if (autofill_wallet_dtc_->state() !=
        syncer::DataTypeController::MODEL_LOADED) {
      return false;
    }
    autofill_wallet_dtc_->StartAssociating(base::BindRepeating(
        &syncer::StartCallbackMock::Run, base::Unretained(&start_callback_)));
    base::RunLoop().RunUntilIdle();
    return true;
  }

  void OnLoadFinished(syncer::ModelType type, const syncer::SyncError& error) {
    last_type_ = type;
    last_error_ = error;
  }

  base::test::TaskEnvironment task_environment_;
  TestingPrefServiceSimple prefs_;
  syncer::UserShare user_share_;
  testing::NiceMock<syncer::MockSyncService> sync_service_;
  syncer::StartCallbackMock start_callback_;
  syncer::FakeSyncableService syncable_service_;
  std::unique_ptr<AutofillWalletDataTypeController> autofill_wallet_dtc_;
  scoped_refptr<FakeWebDataService> web_data_service_;
  testing::NiceMock<syncer::SyncClientMock> sync_client_;

  syncer::ModelType last_type_;
  syncer::SyncError last_error_;
};

TEST_F(AutofillWalletDataTypeControllerTest, StartDatatypeEnabled) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::OK, testing::_, testing::_));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  EXPECT_FALSE(last_error_.IsSet());
  EXPECT_EQ(syncer::AUTOFILL_WALLET_METADATA, last_type_);
  EXPECT_EQ(syncer::DataTypeController::RUNNING, autofill_wallet_dtc_->state());
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByWalletImportWhileRunning) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::OK, testing::_, testing::_));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  EXPECT_EQ(syncer::DataTypeController::RUNNING, autofill_wallet_dtc_->state());
  EXPECT_FALSE(last_error_.IsSet());
  EXPECT_EQ(syncer::AUTOFILL_WALLET_METADATA, last_type_);

  EXPECT_CALL(sync_service_,
              DataTypePreconditionChanged(syncer::AUTOFILL_WALLET_METADATA));
  autofill::prefs::SetPaymentsIntegrationEnabled(&prefs_, false);
  autofill::prefs::SetCreditCardAutofillEnabled(&prefs_, true);
  EXPECT_EQ(
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData,
      autofill_wallet_dtc_->GetPreconditionState());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByCreditCardsWhileRunning) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  EXPECT_CALL(start_callback_,
              Run(syncer::DataTypeController::OK, testing::_, testing::_));

  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  EXPECT_EQ(syncer::DataTypeController::RUNNING, autofill_wallet_dtc_->state());
  EXPECT_FALSE(last_error_.IsSet());
  EXPECT_EQ(syncer::AUTOFILL_WALLET_METADATA, last_type_);

  EXPECT_CALL(sync_service_,
              DataTypePreconditionChanged(syncer::AUTOFILL_WALLET_METADATA));
  autofill::prefs::SetPaymentsIntegrationEnabled(&prefs_, true);
  autofill::prefs::SetCreditCardAutofillEnabled(&prefs_, false);
  EXPECT_EQ(
      syncer::DataTypeController::PreconditionState::kMustStopAndClearData,
      autofill_wallet_dtc_->GetPreconditionState());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByWalletImportAtStartup) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  autofill::prefs::SetPaymentsIntegrationEnabled(&prefs_, false);
  autofill::prefs::SetCreditCardAutofillEnabled(&prefs_, true);
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  base::RunLoop().RunUntilIdle();
  // OnLoadFinished() should not have been called.
  EXPECT_EQ(syncer::UNSPECIFIED, last_type_);
}

TEST_F(AutofillWalletDataTypeControllerTest,
       DatatypeDisabledByCreditCardsAtStartup) {
  SetStartExpectations();
  web_data_service_->LoadDatabase();
  autofill::prefs::SetPaymentsIntegrationEnabled(&prefs_, true);
  autofill::prefs::SetCreditCardAutofillEnabled(&prefs_, false);
  EXPECT_EQ(syncer::DataTypeController::NOT_RUNNING,
            autofill_wallet_dtc_->state());
  Start();
  base::RunLoop().RunUntilIdle();
  // OnLoadFinished() should not have been called.
  EXPECT_EQ(syncer::UNSPECIFIED, last_type_);
}

}  // namespace

}  // namespace browser_sync
