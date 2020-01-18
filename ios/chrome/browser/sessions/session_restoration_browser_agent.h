// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_BROWSER_AGENT_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "ios/chrome/browser/main/browser_user_data.h"

@class SessionWindowIOS;
@class SessionIOSFactory;
class SessionRestorationObserver;
class WebStateList;
@class SessionServiceIOS;

namespace ios {
class ChromeBrowserState;
}

// This class is responsible for handling requests of session restoration. It
// can be observed via SeassonRestorationObserver which it uses to notify
// observers of session restoration events.
class SessionRestorationBrowserAgent
    : public BrowserUserData<SessionRestorationBrowserAgent> {
 public:
  // Creates an SessionRestorationBrowserAgent scoped to |browser|.
  static void CreateForBrowser(Browser* browser,
                               SessionServiceIOS* session_service);

  ~SessionRestorationBrowserAgent() override;

  // Adds/Removes Observer to session restoration events.
  void AddObserver(SessionRestorationObserver* observer);
  void RemoveObserver(SessionRestorationObserver* observer);

  // Restores the |window| (for example, after a crash). If there is only one ,
  // tab showing the NTP, then this tab should be clobbered, otherwise, the tabs
  // from the restored sessions should be added at the end of the current list
  // of tabs. Returns YES if the single NTP tab is closed.
  bool RestoreSessionWindow(SessionWindowIOS* window);

  // Persists the current list of tabs to disk, either immediately or deferred
  // based on the value of |immediately|.
  void SaveSession(const bool immediately);

  // Returns true if there is a session restoration in progress, otherwise it
  // returns false. Note that this method can be called from the UI Thread.
  // This method exists as a work around for crbug.com/763964.
  bool IsRestoringSession();

 private:
  SessionRestorationBrowserAgent(Browser* browser,
                                 SessionServiceIOS* session_service);

  friend class BrowserUserData<SessionRestorationBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // Returns true if the current session can be saved.
  bool CanSaveSession();

  // The service object which handles the actual saving of sessions.
  SessionServiceIOS* session_service_;

  // The list of web states to be saved.
  WebStateList* web_state_list_;

  base::ObserverList<SessionRestorationObserver, true> observers_;

  ios::ChromeBrowserState* browser_state_;

  // Session Factory used to create session data for saving.
  SessionIOSFactory* session_ios_factory_;

  // true when session restoration is in progress.
  bool restoring_session_ = false;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_BROWSER_AGENT_H_
