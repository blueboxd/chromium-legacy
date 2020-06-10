// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_CHECK_ANDROID_BULK_LEAK_CHECK_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_CHECK_ANDROID_BULK_LEAK_CHECK_CONTROLLER_ANDROID_H_

#include "base/observer_list.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"

// This controller allows Android code to interact with the bulk credential leak
// check. Supported interactions include starting the password check, as well as
// getting notified when the state is changed and when each credential is
// checked.
class BulkLeakCheckControllerAndroid {
 public:
  class Observer : public base::CheckedObserver {
   public:
    using DoneCount = util::StrongAlias<class DoneCountTag, int>;
    using TotalCount = util::StrongAlias<class TotalCountTag, int>;

    // Invoked on every observer whenever the state of the bulk leak check
    // changes.
    virtual void OnStateChanged(
        password_manager::BulkLeakCheckService::State state) = 0;

    // Invoked on every observer whenever a new credential is successfully
    // checked.
    virtual void OnCredentialDone(
        const password_manager::LeakCheckCredential& credential,
        password_manager::IsLeaked is_leaked,
        DoneCount credentials_checked,
        TotalCount total_to_check) = 0;
  };

  BulkLeakCheckControllerAndroid();
  ~BulkLeakCheckControllerAndroid();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  // Starts the bulk passwords check using all the saved credentials in the
  // user's password store.
  void StartPasswordCheck();

  // Returns the total number of passwords saved by the user.
  int GetNumberOfSavedPasswords();

  // Returns the last known number of leaked password as of the latest check.
  // Does not affect the state of the bulk leak check.
  int GetNumberOfLeaksFromLastCheck();

 private:
  base::ObserverList<Observer> observers_;
};

#endif  // CHROME_BROWSER_PASSWORD_CHECK_ANDROID_BULK_LEAK_CHECK_CONTROLLER_ANDROID_H_
