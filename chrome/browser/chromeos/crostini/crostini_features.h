// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FEATURES_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FEATURES_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class Profile;

namespace crostini {

// CrostiniFeatures provides an interface for querying which parts of crostini
// are enabled or allowed.
class CrostiniFeatures {
 public:
  static CrostiniFeatures* Get();

  // Returns true if crostini is allowed to run for |profile|.
  // Otherwise, returns false, e.g. if crostini is not available on the device,
  // or it is in the flow to set up managed account creation.
  virtual bool IsAllowed(Profile* profile);

  // When |check_policy| is true, returns true if fully interactive crostini UI
  // may be shown. Implies crostini is allowed to run.
  // When check_policy is false, returns true if crostini UI is not forbidden by
  // hardware, flags, etc, even if it is forbidden by the enterprise policy. The
  // UI uses this to indicate that crostini is available but disabled by policy.
  virtual bool IsUIAllowed(Profile*, bool check_policy = true);

  // Returns whether if Crostini has been enabled, i.e. the user has launched it
  // at least once and not deleted it.
  virtual bool IsEnabled(Profile* profile);

  // Returns true if policy allows export import UI.
  virtual bool IsExportImportUIAllowed(Profile*);

  // Returns whether user is allowed root access to Crostini. Always returns
  // true when advanced access controls feature flag is disabled.
  virtual bool IsRootAccessAllowed(Profile*);

  // Returns true if container upgrade ui is allowed by flag.
  virtual bool IsContainerUpgradeUIAllowed(Profile*);

  using CanChangeAdbSideloadingCallback =
      base::OnceCallback<void(bool can_change_adb_sideloading)>;

  // Checks whether the user is allowed to enable and disable ADB sideloading
  // based on whether the user is the owner, whether the user and the device
  // are managed, and feature flag and policies for managed case. Once this is
  // established, the callback is invoked and passed a boolean indicating
  // whether changes to ADB sideloading are allowed.
  virtual void CanChangeAdbSideloading(
      Profile* profile,
      CanChangeAdbSideloadingCallback callback);

  // Returns whether the user is allowed to configure port forwarding into
  // Crostini. If the user is not managed or if the policy is unset or true,
  // then this returns true, else if the policy is set to false, this returns
  // false.
  virtual bool IsPortForwardingAllowed(Profile* profile);

  // TODO(crbug.com/1004708): Move other functions from crostini_util to here.

 protected:
  static void SetForTesting(CrostiniFeatures* features);

  CrostiniFeatures();
  virtual ~CrostiniFeatures();

 private:
  base::WeakPtrFactory<CrostiniFeatures> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniFeatures);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_FEATURES_H_
