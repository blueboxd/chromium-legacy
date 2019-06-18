// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_FACTORY_H_

#include <map>
#include <memory>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

class AccountId;
class Profile;

namespace chromeos {

class BulkPrintersCalculator;

// Dispenses BulkPrintersCalculator objects based on account id.  Access to this
// object should be sequenced.
class BulkPrintersCalculatorFactory {
 public:
  static BulkPrintersCalculatorFactory* Get();

  // Returns a WeakPtr to the BulkPrintersCalculator registered for
  // |account_id|. If an BulkPrintersCalculator does not exist, one will be
  // created for |account_id|. The returned object remains valid until
  // RemoveForUserId or Shutdown is called.
  base::WeakPtr<BulkPrintersCalculator> GetForAccountId(
      const AccountId& account_id);

  // Returns a WeakPtr to the BulkPrintersCalculator registered for |profile|
  // which could be null if |profile| does not map to a valid AccountId. The
  // returned object remains valid until RemoveForUserId or Shutdown is called.
  base::WeakPtr<BulkPrintersCalculator> GetForProfile(Profile* profile);

  // Returns a WeakPtr to the BulkPrintersCalculator registered for the device.
  base::WeakPtr<BulkPrintersCalculator> GetForDevice();

  // Deletes the BulkPrintersCalculator registered for |account_id|.
  void RemoveForUserId(const AccountId& account_id);

  // Tear down all BulkPrintersCalculator created for users/profiles.
  void ShutdownProfiles();

  // Tear down BulkPrintersCalculator created for the device.
  void ShutdownForDevice();

 private:
  friend struct base::LazyInstanceTraitsBase<BulkPrintersCalculatorFactory>;

  BulkPrintersCalculatorFactory();
  ~BulkPrintersCalculatorFactory();

  std::map<AccountId, std::unique_ptr<BulkPrintersCalculator>>
      printers_by_user_;
  std::unique_ptr<BulkPrintersCalculator> device_printers_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(BulkPrintersCalculatorFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_BULK_PRINTERS_CALCULATOR_FACTORY_H_
