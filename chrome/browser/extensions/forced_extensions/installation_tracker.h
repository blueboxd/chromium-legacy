// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
#define CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_

#include <map>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observer.h"
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
  class Observer : public base::CheckedObserver {
   public:
    // Called after every force-installed extension is loaded (not only
    // installed) or reported as failure.
    //
    // If there are no force-installed extensions configured, this method still
    // gets called.
    virtual void OnForceInstallationFinished() = 0;
  };

  InstallationTracker(ExtensionRegistry* registry, Profile* profile);

  ~InstallationTracker() override;

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;

  void OnShutdown(ExtensionRegistry*) override;

  // InstallationReporter::Observer overrides:
  void OnExtensionInstallationFailed(
      const ExtensionId& extension_id,
      InstallationReporter::FailureReason reason) override;

  // Returns true if all extensions are loaded/failed loading.
  bool IsComplete() const;

  // Add/remove observers to this object, to get notified when installation is
  // finished.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

  const std::map<ExtensionId, ExtensionInfo>& extensions() const {
    return extensions_;
  }

 private:
  // Fire OnForceInstallationFinished() on observers.
  void NotifyInstallationFinished();

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

  // Unowned, but guaranteed to outlive this object.
  ExtensionRegistry* registry_;
  Profile* profile_;
  PrefService* pref_service_;

  PrefChangeRegistrar pref_change_registrar_;

  // Collection of all extensions we are interested in here. Don't update
  // directly, use AddExtensionInfo/RemoveExtensionInfo/ChangeExtensionStatus
  // methods, as |pending_extension_counter_| has to be in sync with contents of
  // this collection.
  std::map<ExtensionId, ExtensionInfo> extensions_;

  // Number of extensions in |extensions_| with status equals to |PENDING|.
  size_t pending_extensions_counter_ = 0;

  // Tracks whether non-empty forcelist policy was received at least once.
  bool loaded_ = false;

  // Tracks whether all extensions are done installing/loading.
  bool complete_ = false;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_{this};
  ScopedObserver<InstallationReporter, InstallationReporter::Observer>
      reporter_observer_{this};

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstallationTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
