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
#include "components/policy/core/common/policy_service.h"
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
                            public InstallationReporter::Observer,
                            public policy::PolicyService::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called after every force-installed extension is loaded (not yet
    // installed) or reported as failure.
    //
    // Called exactly once, during startup (may take several minutes). Use
    // IsDoneLoading() to know if it has already been called. If there are no
    // force-installed extensions configured, this method still gets called.
    virtual void OnForceInstalledExtensionsLoaded() {}

    // Same as OnForceInstalledExtensionsLoaded(), but after they're ready
    // instead of loaded.
    //
    // Called exactly once, during startup (may take several minutes). Use
    // IsReady() to know if it has already been called. If there are no
    // force-installed extensions configured, this method still gets called.
    virtual void OnForceInstalledExtensionsReady() {}
  };

  InstallationTracker(ExtensionRegistry* registry, Profile* profile);

  ~InstallationTracker() override;

  // Returns true if all extensions loaded/failed loading.
  bool IsDoneLoading() const;

  // Returns true if all extensions installed/failed installing.
  bool IsReady() const;

  // Add/remove observers to this object, to get notified when installation is
  // finished.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionReady(content::BrowserContext* browser_context,
                        const Extension* extension) override;
  void OnShutdown(ExtensionRegistry*) override;

  // InstallationReporter::Observer overrides:
  void OnExtensionInstallationFailed(
      const ExtensionId& extension_id,
      InstallationReporter::FailureReason reason) override;

  // policy::PolicyService::Observer overrides:
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  void OnPolicyServiceInitialized(policy::PolicyDomain domain) override;

  enum class ExtensionStatus {
    // Extension appears in force-install list, but it not installed yet.
    PENDING,

    // Extension was successfully loaded.
    LOADED,

    // Extension is ready. This happens after loading.
    READY,

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
  policy::PolicyService* policy_service();

  // Fires OnForceInstallationFinished() on observers, then changes |status_| to
  // kComplete.
  void MaybeNotifyObservers();

  // Increment (or decrement) |load_pending_count_| and |install_pending_count_|
  // by |delta|, depending on |status|.
  void UpdateCounters(ExtensionStatus status, int delta);

  // Helper method to modify |extensions_| and bounded counter, adds extension
  // to the collection.
  void AddExtensionInfo(const ExtensionId& extension_id,
                        ExtensionStatus status,
                        bool is_from_store);

  // Helper method to modify |extensions_| and bounded counter, changes status
  // of one extensions.
  void ChangeExtensionStatus(const ExtensionId& extension_id,
                             ExtensionStatus status);

  // Loads list of force-installed extensions if available. Only called once.
  void OnForcedExtensionsPrefReady();

  // Unowned, but guaranteed to outlive this object.
  ExtensionRegistry* registry_;
  Profile* profile_;
  PrefService* pref_service_;

  // Collection of all extensions we are interested in here. Don't update
  // directly, use AddExtensionInfo/RemoveExtensionInfo/ChangeExtensionStatus
  // methods, as |pending_extension_counter_| has to be in sync with contents of
  // this collection.
  std::map<ExtensionId, ExtensionInfo> extensions_;

  // Number of extensions in |extensions_| with status |PENDING|.
  size_t load_pending_count_ = 0;
  // Number of extensions in |extensions_| with status |PENDING| or |LOADED|.
  // (ie. could be loaded, but not ready yet).
  size_t ready_pending_count_ = 0;

  // Stores the current state of this tracker, to know when it's complete, and
  // to perform sanity DCHECK()s.
  enum Status {
    // Waiting for PolicyService to finish initializing. Listening for
    // OnPolicyServiceInitialized().
    kWaitingForPolicyService,
    // Waiting for one or more extensions to finish loading. Listening for
    // |ExtensionRegistryObserver| events.
    kWaitingForExtensionLoads,
    // Waiting for one or more extensions to finish loading. Listening for
    // |ExtensionRegistryObserver| events. Extensions have already finished
    // loading; we're still waiting for the "ready" state. IsDoneLoading()
    // returns true, but IsReady() returns false.
    kWaitingForExtensionReady,
    // All extensions have finished installing (successfully or not); observers
    // have been called exactly once, and IsDoneLoading() and IsReady()
    // both return true.
    kComplete,
  };
  Status status_ = kWaitingForPolicyService;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observer_{this};
  ScopedObserver<InstallationReporter, InstallationReporter::Observer>
      reporter_observer_{this};

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(InstallationTracker);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_FORCED_EXTENSIONS_INSTALLATION_TRACKER_H_
