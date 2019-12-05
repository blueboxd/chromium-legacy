// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_

#include <map>
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
  enum class ExtensionStatus {
    // Extension appears in force-install list, but it not installed yet.
    PENDING,

    // Extension was successfully loaded.
    LOADED,

    // Extension installation failure was reported.
    FAILED
  };

  // Helper struct with supplementary info for extensions from force-install
  // list.
  struct ExtensionInfo {
    // Current status of the extension: loaded, failed, or still installing.
    ExtensionStatus status;

    // Additional info: whether extension is from Chrome Web Store, or
    // self-hosted.
    bool is_from_store;
  };

  // Helper method to modify |extensions_| and bounded counter, adds extension
  // to the collection.
  void AddExtensionInfo(const ExtensionId& extension_id,
                        ExtensionStatus status,
                        bool is_from_store);

  // Helper method to modify |extensions_| and bounded counter, changes status
  // of one extensions.
  void ChangeExtensionStatus(const ExtensionId& extension_id,
                             ExtensionStatus status);

  // Helper method to modify |extensions_| and bounded counter, removes
  // extension from the collection.
  void RemoveExtensionInfo(const ExtensionId& extension_id);

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

  // Collection of all extensions we are interested in here. Don't update
  // directly, use AddExtensionInfo/RemoveExtensionInfo/ChangeExtensionStatus
  // methods, as |pending_extension_counter_| has to be in sync with contents of
  // this collection.
  std::map<ExtensionId, ExtensionInfo> extensions_;

  // Number of extensions in |extensions_| with status equals to |PENDING|.
  size_t pending_extensions_counter_ = 0;

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
