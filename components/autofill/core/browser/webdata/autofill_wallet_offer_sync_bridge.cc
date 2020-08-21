// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_offer_sync_bridge.h"

#include <utility>

#include "base/logging.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"

namespace autofill {

namespace {

// Address to this variable used as the user data key.
static int kAutofillWalletOfferSyncBridgeUserDataKey = 0;

}  // namespace

// static
void AutofillWalletOfferSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataBackend* web_data_backend,
    AutofillWebDataService* web_data_service) {
  web_data_service->GetDBUserData()->SetUserData(
      &kAutofillWalletOfferSyncBridgeUserDataKey,
      std::make_unique<AutofillWalletOfferSyncBridge>(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL_WALLET_OFFER,
              /*dump_stack=*/base::RepeatingClosure())));
}

// static
syncer::ModelTypeSyncBridge* AutofillWalletOfferSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutofillWalletOfferSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          &kAutofillWalletOfferSyncBridgeUserDataKey));
}

AutofillWalletOfferSyncBridge::AutofillWalletOfferSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)) {}

AutofillWalletOfferSyncBridge::~AutofillWalletOfferSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
AutofillWalletOfferSyncBridge::CreateMetadataChangeList() {
  NOTIMPLEMENTED();
  return nullptr;
}

base::Optional<syncer::ModelError> AutofillWalletOfferSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

base::Optional<syncer::ModelError>
AutofillWalletOfferSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  NOTIMPLEMENTED();
  return base::nullopt;
}

void AutofillWalletOfferSyncBridge::GetData(StorageKeyList storage_keys,
                                            DataCallback callback) {
  NOTIMPLEMENTED();
}

void AutofillWalletOfferSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  NOTIMPLEMENTED();
}

std::string AutofillWalletOfferSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

std::string AutofillWalletOfferSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTIMPLEMENTED();
  return "";
}

bool AutofillWalletOfferSyncBridge::SupportsIncrementalUpdates() const {
  return false;
}

void AutofillWalletOfferSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  NOTIMPLEMENTED();
}

}  // namespace autofill
