// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"

#include "base/lazy_instance.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace chromeos {

namespace {

base::LazyInstance<BulkPrintersCalculatorFactory>::DestructorAtExit
    g_printers_factory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
BulkPrintersCalculatorFactory* BulkPrintersCalculatorFactory::Get() {
  return g_printers_factory.Pointer();
}

base::WeakPtr<BulkPrintersCalculator>
BulkPrintersCalculatorFactory::GetForAccountId(const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto found = printers_by_user_.find(account_id);
  if (found != printers_by_user_.end()) {
    return found->second->AsWeakPtr();
  }

  printers_by_user_[account_id] = BulkPrintersCalculator::Create();
  return printers_by_user_[account_id]->AsWeakPtr();
}

base::WeakPtr<BulkPrintersCalculator>
BulkPrintersCalculatorFactory::GetForProfile(Profile* profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return nullptr;

  return GetForAccountId(user->GetAccountId());
}

void BulkPrintersCalculatorFactory::RemoveForUserId(
    const AccountId& account_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printers_by_user_.erase(account_id);
}

base::WeakPtr<BulkPrintersCalculator>
BulkPrintersCalculatorFactory::GetForDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!device_printers_)
    device_printers_ = BulkPrintersCalculator::Create();
  return device_printers_->AsWeakPtr();
}

void BulkPrintersCalculatorFactory::ShutdownProfiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  printers_by_user_.clear();
}

void BulkPrintersCalculatorFactory::ShutdownForDevice() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  device_printers_.reset();
}

BulkPrintersCalculatorFactory::BulkPrintersCalculatorFactory() = default;
BulkPrintersCalculatorFactory::~BulkPrintersCalculatorFactory() = default;

}  // namespace chromeos
