// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_H_
#define COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/prefs/pref_service.h"

namespace safety_check {

// Class for performing browser safety checks common to desktop, Android, and
// iOS. Platform-specific checks, such as updates and extensions, are
// implemented in handlers.
class SafetyCheck {
 public:
  // The following enums represent the state of each component (common among
  // desktop, Android, and iOS) of the safety check and should be kept in sync
  // with the JS frontend (safety_check_browser_proxy.js) and |SafetyCheck*|
  // metrics enums in enums.xml.
  enum class SafeBrowsingStatus {
    kChecking = 0,
    kEnabled = 1,
    kDisabled = 2,
    kDisabledByAdmin = 3,
    kDisabledByExtension = 4,
    kEnabledStandard = 5,
    kEnabledEnhanced = 6,
    // New enum values must go above here.
    kMaxValue = kEnabledEnhanced,
  };

  class SafetyCheckHandlerInterface {
   public:
    virtual void OnSafeBrowsingCheckResult(SafeBrowsingStatus status) = 0;
  };

  explicit SafetyCheck(SafetyCheckHandlerInterface* handler);
  ~SafetyCheck();

  // Gets the status of Safe Browsing from the PrefService and invokes
  // OnSafeBrowsingCheckResult on each Observer with results.
  void CheckSafeBrowsing(PrefService* pref_service);

 private:
  SafetyCheckHandlerInterface* handler_;
};

}  // namespace safety_check

#endif  // COMPONENTS_SAFETY_CHECK_SAFETY_CHECK_H_
