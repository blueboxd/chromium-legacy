// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_OBSERVER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_OBSERVER_H_

#include "chrome/browser/ui/webui/help/version_updater.h"
#include "chrome/browser/ui/webui/settings/safety_check_handler.h"

// Observer for SafetyCheckHandler events. Currently only used for testing.
class SafetyCheckHandlerObserver {
 public:
  SafetyCheckHandlerObserver() = default;
  SafetyCheckHandlerObserver(const SafetyCheckHandlerObserver&) = delete;
  SafetyCheckHandlerObserver& operator=(const SafetyCheckHandlerObserver&) =
      delete;
  virtual ~SafetyCheckHandlerObserver() = default;

  virtual void OnUpdateCheckStart() = 0;
  virtual void OnUpdateCheckResult(SafetyCheckHandler::UpdateStatus status) = 0;
  virtual void OnSafeBrowsingCheckStart() = 0;
  virtual void OnSafeBrowsingCheckResult(
      SafetyCheckHandler::SafeBrowsingStatus status) = 0;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SAFETY_CHECK_HANDLER_OBSERVER_H_
