// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This interface is for managing the global services of the application.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PROCESS_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PROCESS_H_

#include "android_webview/browser/aw_feature_list_creator.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_ui_manager.h"
#include "android_webview/browser/safe_browsing/aw_safe_browsing_whitelist_manager.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/triggers/trigger_manager.h"

namespace android_webview {

class AwBrowserProcess {
 public:
  AwBrowserProcess(AwFeatureListCreator* aw_feature_list_creator);
  ~AwBrowserProcess();

  static AwBrowserProcess* GetInstance();

  PrefService* local_state();

  void CreateLocalState();
  void InitSafeBrowsing();

  safe_browsing::RemoteSafeBrowsingDatabaseManager* GetSafeBrowsingDBManager();

  // Called on UI thread.
  // This method lazily creates TriggerManager.
  // Needs to happen after |safe_browsing_ui_manager_| is created.
  safe_browsing::TriggerManager* GetSafeBrowsingTriggerManager();

  // InitSafeBrowsing must be called first.
  // Called on UI and IO threads.
  AwSafeBrowsingWhitelistManager* GetSafeBrowsingWhitelistManager() const;

  // InitSafeBrowsing must be called first.
  // Called on UI and IO threads.
  AwSafeBrowsingUIManager* GetSafeBrowsingUIManager() const;

 private:
  void CreateSafeBrowsingUIManager();
  void CreateSafeBrowsingWhitelistManager();

  // If non-null, this object holds a pref store that will be taken by
  // AwBrowserProcess to create the |local_state_|.
  // The AwFeatureListCreator is owned by AwMainDelegate.
  AwFeatureListCreator* aw_feature_list_creator_;

  std::unique_ptr<PrefService> local_state_;

  // Accessed on both UI and IO threads.
  scoped_refptr<AwSafeBrowsingUIManager> safe_browsing_ui_manager_;

  // Accessed on UI thread only.
  std::unique_ptr<safe_browsing::TriggerManager> safe_browsing_trigger_manager_;

  // These two are accessed on IO thread only.
  scoped_refptr<safe_browsing::RemoteSafeBrowsingDatabaseManager>
      safe_browsing_db_manager_;
  bool safe_browsing_db_manager_started_ = false;

  // TODO(amalova): Consider to make WhitelistManager per-profile.
  // Accessed on UI and IO threads.
  std::unique_ptr<AwSafeBrowsingWhitelistManager>
      safe_browsing_whitelist_manager_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserProcess);
};

}  // namespace android_webview
#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_PROCESS_H_
