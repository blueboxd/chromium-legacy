// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_

#include <set>
#include <string>

#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class PrefService;
class Profile;

namespace content {
class BrowserContext;
}

namespace extensions {

// Used to track installation of force-installed extensions for the profile
// and report stats to UMA.
// ExtensionService owns this class and outlives it.
class InstallationTracker : public ExtensionRegistryObserver,
                            public InstallationReporter::Observer {
 public:
  InstallationTracker(ExtensionRegistry* registry,
                      Profile* profile,
                      std::unique_ptr<base::OneShotTimer> timer =
                          std::make_unique<base::OneShotTimer>());

  ~InstallationTracker() override;

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

  void OnShutdown(ExtensionRegistry*) override;

  // InstallationReporter::Observer overrides:
  void OnExtensionInstallationFailed(
      const ExtensionId& extension_id,
      InstallationReporter::FailureReason reason) override;

 private:
  // Loads list of force-installed extensions if available.
  void OnForcedExtensionsPrefChanged();

  // If |succeeded| report time elapsed for extensions load,
  // otherwise amount of not yet loaded extensions and reasons
  // why they were not installed.
  void ReportResults();

  // Unowned, but guaranteed to outlive this object.
  ExtensionRegistry* registry_;
  Profile* profile_;
  // Unowned, but guaranteed to outlive this object.
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Moment when the class was initialized.
  base::Time start_time_;

  // Set of all extensions requested to be force installed.
  std::set<std::string> forced_extensions_;

  // Set of extension, which are neither yet loaded not yet failed permanently.
  std::set<std::string> pending_forced_extensions_;

  // Set of extensions, which are not loaded (both not loaded yet and failed).
  std::set<std::string> failed_forced_extensions_;

  // Tracks whether non-empty forcelist policy was received at least once.
  bool loaded_ = false;

  // Tracks whether stats were already reported for the session.
  bool reported_ = false;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer_{this};

  // Tracks installation reporting timeout.
  std::unique_ptr<base::OneShotTimer> timer_;

  DISALLOW_COPY_AND_ASSIGN(InstallationTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
