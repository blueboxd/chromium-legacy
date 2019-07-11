// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TABLET_MODE_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_TABLET_MODE_CLIENT_H_

#include <memory>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/browser_tab_strip_tracker_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

class BrowserTabStripTracker;
class TabletModeClientObserver;

// Holds tablet mode state in chrome. Observes ash for changes, then
// synchronously fires all its observers. This allows all tablet mode code in
// chrome to see a state change at the same time.
class TabletModeClient : public ash::TabletModeObserver,
                         public BrowserTabStripTrackerDelegate,
                         public TabStripModelObserver {
 public:
  TabletModeClient();
  ~TabletModeClient() override;

  // Initializes and connects to ash.
  void Init();

  static TabletModeClient* Get();

  bool tablet_mode_enabled() const { return tablet_mode_enabled_; }

  // Adds the observer and immediately triggers it with the initial state.
  void AddObserver(TabletModeClientObserver* observer);

  void RemoveObserver(TabletModeClientObserver* observer);

  // Notify the tablet mode change.
  void OnTabletModeToggled(bool enabled);

  // ash::TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // BrowserTabStripTrackerDelegate:
  bool ShouldTrackBrowser(Browser* browser) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  // Enables/disables mobile-like behavior for webpages in existing browsers, as
  // well as starts observing new browser pages if |enabled| is true.
  void SetMobileLikeBehaviorEnabled(bool enabled);

  bool tablet_mode_enabled_ = false;

  // We only override the WebKit preferences of webcontents that belong to
  // tabstrips in browsers. When a webcontents is newly created, its WebKit
  // preferences are refreshed *before* it's added to any tabstrip, hence
  // ChromeContentBrowserClientChromeOsPart::OverrideWebkitPrefs() wouldn't be
  // able to override the mobile-like behavior prefs we want. Therefore, we need
  // to observe webcontents being added to the tabstrips in order to trigger
  // a refresh of its WebKit prefs.
  std::unique_ptr<BrowserTabStripTracker> tab_strip_tracker_;

  base::ObserverList<TabletModeClientObserver,
                     true /* check_empty */>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeClient);
};

#endif  // CHROME_BROWSER_UI_ASH_TABLET_MODE_CLIENT_H_
