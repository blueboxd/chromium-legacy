// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/strike_database_integrator_test_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"

namespace autofill {

const char kProjectPrefix[] = "StrikeDatabaseIntegratorTest";
const int kMaxStrikesLimit = 6;

StrikeDatabaseIntegratorTestStrikeDatabase::
    StrikeDatabaseIntegratorTestStrikeDatabase(StrikeDatabase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

StrikeDatabaseIntegratorTestStrikeDatabase::
    ~StrikeDatabaseIntegratorTestStrikeDatabase() {}

std::string StrikeDatabaseIntegratorTestStrikeDatabase::GetProjectPrefix() {
  return kProjectPrefix;
}

int StrikeDatabaseIntegratorTestStrikeDatabase::GetMaxStrikesLimit() {
  return kMaxStrikesLimit;
}

int64_t StrikeDatabaseIntegratorTestStrikeDatabase::GetExpiryTimeMicros() {
  // Expiry time is 1 year.
  return (int64_t)1000000 * 60 * 60 * 24 * 365;
}

bool StrikeDatabaseIntegratorTestStrikeDatabase::UniqueIdsRequired() {
  return unique_ids_required_;
}

void StrikeDatabaseIntegratorTestStrikeDatabase::SetUniqueIdsRequired(
    bool unique_ids_required) {
  unique_ids_required_ = unique_ids_required;
}

}  // namespace autofill
