// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/local_card_migration_strike_database.h"
#include <limits>

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const int LocalCardMigrationStrikeDatabase::kStrikesToRemoveWhenLocalCardAdded =
    2;
const int LocalCardMigrationStrikeDatabase::kStrikesToAddWhenBubbleClosed = 3;
const int LocalCardMigrationStrikeDatabase::kStrikesToAddWhenDialogClosed = 6;

LocalCardMigrationStrikeDatabase::LocalCardMigrationStrikeDatabase(
    StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

LocalCardMigrationStrikeDatabase::~LocalCardMigrationStrikeDatabase() {}

std::string LocalCardMigrationStrikeDatabase::GetProjectPrefix() {
  return "LocalCardMigration";
}

int LocalCardMigrationStrikeDatabase::GetMaxStrikesLimit() {
  return 6;
}

int64_t LocalCardMigrationStrikeDatabase::GetExpiryTimeMicros() {
  // Ideally, we should be able to annotate cards deselected at migration time
  // as cards the user is not interested in uploading.  Until then, we have been
  // asked to not expire local card migration strikes based on a time limit.
  // This option does not yet exist, so as a workaround the expiry time is set
  // to the maximum amount (roughly 292,000 years).
  // TODO(jsaul): Create an option to disable expiry time completely.
  return std::numeric_limits<int64_t>::max();
}

bool LocalCardMigrationStrikeDatabase::UniqueIdsRequired() {
  return false;
}

}  // namespace autofill
