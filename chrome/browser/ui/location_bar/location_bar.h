// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
#define CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

class LocationBarTesting;
class OmniboxView;
class Profile;

namespace content {
class WebContents;
}

// The LocationBar class is a virtual interface, defining access to the
// window's location bar component.  This class exists so that cross-platform
// components like the browser command system can talk to the platform
// specific implementations of the location bar control.  It also allows the
// location bar to be mocked for testing.
class LocationBar {
 public:
  explicit LocationBar(Profile* profile);

  // The details necessary to open the user's desired omnibox match.
  virtual GURL GetDestinationURL() const = 0;
  virtual WindowOpenDisposition GetWindowOpenDisposition() const = 0;
  virtual ui::PageTransition GetPageTransition() const = 0;
  virtual base::TimeTicks GetMatchSelectionTimestamp() const = 0;

  // Accepts the current string of text entered in the location bar.
  virtual void AcceptInput() = 0;

  // Accepts the current string of text entered in the location bar. If
  // |match_selection_timestamp| is not null, uses this value to track
  // latency of page loads starting at user input.
  virtual void AcceptInput(base::TimeTicks match_selection_timestamp) = 0;

  // Focuses the location bar.  Optionally also selects its contents.
  //
  // User-initiated focuses should have |select_all| set to true, as users
  // are accustomed to being able to use Ctrl+L to select-all in the omnibox.
  //
  // Renderer-initiated focuses should have |select_all| set to false, as the
  // user may be in the middle of typing while the tab finishes loading.
  // In that case, we don't want to select-all and cause the user to clobber
  // their already-typed text.
  virtual void FocusLocation(bool select_all) = 0;

  // Puts the user into keyword mode with their default search provider.
  virtual void FocusSearch() = 0;

  // Updates the state of the images showing the content settings status.
  virtual void UpdateContentSettingsIcons() = 0;

  // Updates the visibility of the bookmark star.
  virtual void UpdateBookmarkStarVisibility() = 0;

  // Saves the state of the location bar to the specified WebContents, so that
  // it can be restored later. (Done when switching tabs).
  virtual void SaveStateToContents(content::WebContents* contents) = 0;

  // Reverts the location bar.  The bar's permanent text will be shown.
  virtual void Revert() = 0;

  virtual const OmniboxView* GetOmniboxView() const = 0;
  virtual OmniboxView* GetOmniboxView() = 0;

  // Returns a pointer to the testing interface.
  virtual LocationBarTesting* GetLocationBarForTesting() = 0;

  Profile* profile() const { return profile_; }

 protected:
  virtual ~LocationBar();

  // Checks if any extension has requested that the bookmark star be hidden.
  bool IsBookmarkStarHiddenByExtension() const;

 private:
  class ExtensionLoadObserver;

  Profile* profile_;

  std::unique_ptr<ExtensionLoadObserver> extension_load_observer_;

  DISALLOW_COPY_AND_ASSIGN(LocationBar);
};

class LocationBarTesting {
 public:
  // Returns whether or not the bookmark star decoration is visible.
  virtual bool GetBookmarkStarVisibility() = 0;

  // Invokes the content setting image at |index|, displaying the bubble.
  // Returns false if there is none.
  virtual bool TestContentSettingImagePressed(size_t index) = 0;

  // Returns if the content setting image at |index| is displaying a bubble.
  virtual bool IsContentSettingBubbleShowing(size_t index) = 0;

 protected:
  virtual ~LocationBarTesting() {}
};

#endif  // CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_H_
