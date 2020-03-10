// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"

// Settings page UI handler that checks four areas of browser safety:
// browser updates, password leaks, malicious extensions, and unwanted
// software.
class SafetyCheckHandler
    : public settings::SettingsPageUIHandler,
      public password_manager::BulkLeakCheckService::Observer {
 public:
  // The following enums represent the state of each component of the safety
  // check and should be kept in sync with the JS frontend
  // (safety_check_browser_proxy.js).
  enum class UpdateStatus {
    kChecking,
    kUpdated,
    kUpdating,
    kRelaunch,
    kDisabledByAdmin,
    kFailedOffline,
    kFailed,
  };
  enum class SafeBrowsingStatus {
    kChecking,
    kEnabled,
    kDisabled,
    kDisabledByAdmin,
    kDisabledByExtension,
  };
  enum class PasswordsStatus {
    kChecking,
    kSafe,
    kCompromisedExist,
    kOffline,
    kNoPasswords,
    kSignedOut,
    kQuotaLimit,
    kTooManyPasswords,
    kError,
  };

  SafetyCheckHandler();
  ~SafetyCheckHandler() override;

  // Triggers all four of the browser safety checks.
  // Note: since the checks deal with sensitive user information, this method
  // should only be called as a result of an explicit user action.
  void PerformSafetyCheck();

 protected:
  SafetyCheckHandler(std::unique_ptr<VersionUpdater> version_updater,
                     password_manager::BulkLeakCheckService* leak_service);

 private:
  // Handles triggering the safety check from the frontend (by user pressing a
  // button).
  void HandlePerformSafetyCheck(const base::ListValue* args);

  // Triggers an update check and invokes OnUpdateCheckResult once results
  // are available.
  void CheckUpdates();

  // Gets the status of Safe Browsing from the PrefService and invokes
  // OnSafeBrowsingCheckResult with results.
  void CheckSafeBrowsing();

  // Triggers a bulk password leak check and invokes OnPasswordsCheckResult once
  // results are available.
  void CheckPasswords();

  // Callbacks that get triggered when each check completes.
  void OnUpdateCheckResult(VersionUpdater::Status status,
                           int progress,
                           bool rollback,
                           const std::string& version,
                           int64_t update_size,
                           const base::string16& message);
  void OnSafeBrowsingCheckResult(SafeBrowsingStatus status);
  void OnPasswordsCheckResult(PasswordsStatus status, int num_compromised);

  // Methods for building user-visible strings based on the safety check
  // state.
  base::string16 GetStringForUpdates(UpdateStatus status);
  base::string16 GetStringForSafeBrowsing(SafeBrowsingStatus status);
  base::string16 GetStringForPasswords(PasswordsStatus status,
                                       int num_compromised);

  // BulkLeakCheckService::Observer implementation.
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  // SettingsPageUIHandler implementation.
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  std::unique_ptr<VersionUpdater> version_updater_;
  password_manager::BulkLeakCheckService* leak_service_ = nullptr;
  ScopedObserver<password_manager::BulkLeakCheckService,
                 password_manager::BulkLeakCheckService::Observer>
      observed_leak_check_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_H_
