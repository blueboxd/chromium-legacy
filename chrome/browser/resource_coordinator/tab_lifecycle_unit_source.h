// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_SOURCE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_SOURCE_H_

#include <memory>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_source_base.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "services/resource_coordinator/public/mojom/lifecycle.mojom.h"

class PrefChangeRegistrar;
class PrefService;
class TabStripModel;

namespace content {
class WebContents;
}

namespace resource_coordinator {

class InterventionPolicyDatabase;
class TabLifecylesEnterprisePreferenceMonitor;
class TabLifecycleObserver;
class TabLifecycleStateObserver;
class TabLifecycleUnitExternal;
class UsageClock;

// Creates and destroys LifecycleUnits as tabs are created and destroyed.
class TabLifecycleUnitSource : public BrowserListObserver,
                               public LifecycleUnitSourceBase,
                               public TabStripModelObserver {
 public:
  class TabLifecycleUnit;
  class LifecycleStateObserver;

  TabLifecycleUnitSource(
      InterventionPolicyDatabase* intervention_policy_database,
      UsageClock* usage_clock);
  ~TabLifecycleUnitSource() override;

  // Should be called once all the dependencies of this class have been created
  // (e.g. the global PerformanceManager instance).
  void Start();

  // Returns the TabLifecycleUnitExternal instance associated with
  // |web_contents|, or nullptr if |web_contents| isn't a tab.
  static TabLifecycleUnitExternal* GetTabLifecycleUnitExternal(
      content::WebContents* web_contents);

  // Adds / removes an observer that is notified when the discarded or auto-
  // discardable state of a tab changes.
  void AddTabLifecycleObserver(TabLifecycleObserver* observer);
  void RemoveTabLifecycleObserver(TabLifecycleObserver* observer);

  // Pretend that |tab_strip| is the TabStripModel of the focused window.
  void SetFocusedTabStripModelForTesting(TabStripModel* tab_strip);

  InterventionPolicyDatabase* intervention_policy_database() const {
    return intervention_policy_database_;
  }

  // Returns the state of the tab lifecycles feature enterprise control. This
  // returns true if the feature should be enabled, false otherwise.
  bool tab_lifecycles_enterprise_policy() const {
    return tab_lifecycles_enterprise_policy_;
  }

 protected:
  class TabLifecycleUnitHolder;

  // LifecycleUnitSourceBase:
  void OnFirstLifecycleUnitCreated() override;
  void OnAllLifecycleUnitsDestroyed() override;

 private:
  friend class TabLifecycleStateObserver;
  friend class TabLifecycleUnitTest;
  friend class TabManagerTest;
  friend class TabActivityWatcherTest;
  FRIEND_TEST_ALL_PREFIXES(TabLifecycleUnitSourceTest,
                           TabProactiveDiscardedByFrozenCallback);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest, TabManagerWasDiscarded);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           TabManagerWasDiscardedCrossSiteSubFrame);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownSingleTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownSharedTabProcess);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownWithUnloadHandler);
  FRIEND_TEST_ALL_PREFIXES(TabManagerTest,
                           ProactiveFastShutdownWithBeforeunloadHandler);

  // Returns the TabLifecycleUnit instance associated with |web_contents|, or
  // nullptr if |web_contents| isn't a tab.
  static TabLifecycleUnit* GetTabLifecycleUnit(
      content::WebContents* web_contents);

  // Returns the TabStripModel of the focused browser window, if any.
  TabStripModel* GetFocusedTabStripModel() const;

  // Updates the focused TabLifecycleUnit.
  void UpdateFocusedTab();

  // Updates the focused TabLifecycleUnit to |new_focused_lifecycle_unit|.
  // TabInsertedAt() calls this directly instead of UpdateFocusedTab() because
  // the active WebContents of a TabStripModel isn't updated when
  // TabInsertedAt() is called.
  void UpdateFocusedTabTo(TabLifecycleUnit* new_focused_lifecycle_unit);

  // Methods called by OnTabStripModelChanged()
  void OnTabInserted(TabStripModel* tab_strip_model,
                     content::WebContents* contents,
                     bool foreground);
  void OnTabDetached(content::WebContents* contents);
  void OnTabReplaced(content::WebContents* old_contents,
                     content::WebContents* new_contents);

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;
  void OnBrowserNoLongerActive(Browser* browser) override;

  // This is called indirectly from the corresponding event on a PageNode in the
  // performance_manager Graph.
  static void OnLifecycleStateChanged(content::WebContents* web_contents,
                                      mojom::LifecycleState state);

  // Callback for TabLifecyclesEnterprisePreferenceMonitor.
  void SetTabLifecyclesEnterprisePolicy(bool enabled);

  // Tracks the BrowserList and all TabStripModels.
  BrowserTabStripTracker browser_tab_strip_tracker_;

  // Pretend that this is the TabStripModel of the focused window, for testing.
  TabStripModel* focused_tab_strip_model_for_testing_ = nullptr;

  // The currently focused TabLifecycleUnit. Updated by UpdateFocusedTab().
  TabLifecycleUnit* focused_lifecycle_unit_ = nullptr;

  // Observers notified when the discarded or auto-discardable state of a tab
  // changes.
  base::ObserverList<TabLifecycleObserver>::Unchecked tab_lifecycle_observers_;

  // The intervention policy database used to assist freezing/discarding
  // decisions.
  InterventionPolicyDatabase* intervention_policy_database_;

  // A clock that advances when Chrome is in use.
  UsageClock* const usage_clock_;

  // The enterprise policy for overriding the tab lifecycles feature.
  bool tab_lifecycles_enterprise_policy_ = true;

  // In official production builds this monitors policy settings and reflects
  // them in |tab_lifecycles_enterprise_policy_|.
  std::unique_ptr<TabLifecylesEnterprisePreferenceMonitor>
      tab_lifecycles_enterprise_preference_monitor_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecycleUnitSource);
};

// Helper class used for getting and monitoring enterprise-policy controlled
// preferences that can control the tab lifecycles feature. Exposed for testing.
class TabLifecylesEnterprisePreferenceMonitor {
 public:
  using OnPreferenceChangedCallback = base::RepeatingCallback<void(bool)>;

  // Creates a preference monitor that monitors the provided PrefService. When
  // the preference is initially checked or changed its value is provided via
  // the provided callback.
  TabLifecylesEnterprisePreferenceMonitor(PrefService* pref_service,
                                          OnPreferenceChangedCallback callback);

  ~TabLifecylesEnterprisePreferenceMonitor();

 private:
  void GetPref();

  PrefService* pref_service_;
  OnPreferenceChangedCallback callback_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TabLifecylesEnterprisePreferenceMonitor);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_LIFECYCLE_UNIT_SOURCE_H_
