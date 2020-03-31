// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_

#include <string>

#include "base/strings/string16.h"
#include "chrome/common/buildflags.h"
#include "extensions/browser/management_policy.h"

namespace extensions {

class Extension;
class ExtensionManagement;

// The standard management policy provider, which takes into account the
// extension black/whitelists and admin black/whitelists.
class StandardManagementPolicyProvider : public ManagementPolicy::Provider {
 public:
#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  // These enum values represent a child user trying to install an extension
  // during the COVID-19 crisis.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserExtensionAllowlist" in
  // src/tools/metrics/histograms/enums.xml.
  enum class UmaExtensionStateAllowlist {
    // Recorded when the extension id is not found in the allowlist.
    kAllowlistMiss = 0,
    // Recorded when the extension id is found in the allowlist.
    kAllowlistHit = 1,
    // Add future entries above this comment, in sync with
    // "SupervisedUserExtensionAllowlist" in
    // src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kAllowlistHit
  };
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  explicit StandardManagementPolicyProvider(
      const ExtensionManagement* settings);

  ~StandardManagementPolicyProvider() override;

  // ManagementPolicy::Provider implementation.
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const Extension* extension,
                   base::string16* error) const override;
  bool UserMayInstall(const Extension* extension,
                      base::string16* error) const override;
  bool UserMayModifySettings(const Extension* extension,
                             base::string16* error) const override;
  bool ExtensionMayModifySettings(const Extension* source_extension,
                                  const Extension* extension,
                                  base::string16* error) const override;
  bool MustRemainEnabled(const Extension* extension,
                         base::string16* error) const override;
  bool MustRemainDisabled(const Extension* extension,
                          disable_reason::DisableReason* reason,
                          base::string16* error) const override;
  bool MustRemainInstalled(const Extension* extension,
                           base::string16* error) const override;
  bool ShouldForceUninstall(const Extension* extension,
                            base::string16* error) const override;

 private:
  const ExtensionManagement* settings_;
  bool ReturnLoadError(const extensions::Extension* extension,
                       base::string16* error) const;

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  bool IsSupervisedUserAllowlistExtensionInstallActive() const;

  // TODO(crbug/1063104): Remove this function once full extensions launches.
  void RecordAllowlistExtensionUmaMetrics(UmaExtensionStateAllowlist state,
                                          const Extension* extension) const;
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_STANDARD_MANAGEMENT_POLICY_PROVIDER_H_
