// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_UTIL_ANDROID_ENTERPRISE_INFO_H_
#define CHROME_BROWSER_ENTERPRISE_UTIL_ANDROID_ENTERPRISE_INFO_H_

#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"

// Class to connect native calls to
// org.chromium.chrome.browser.policy.EnterpriseInfo. This class is only usable
// for Android and is only built for Android.

// Only use from the UI Thread.

namespace chrome {
namespace enterprise_util {

class AndroidEnterpriseInfoFriendHelper;

class AndroidEnterpriseInfo {
 public:
  using EnterpriseInfoCallback = base::OnceCallback<void(bool, bool)>;
  ~AndroidEnterpriseInfo();

  static AndroidEnterpriseInfo* GetInstance() {
    static base::NoDestructor<AndroidEnterpriseInfo> instance;
    return instance.get();
  }

  // Request the owned state from
  // org.chromium.chrome.browser.policy.EnterpriseInfo and notify |callback|
  // when the request is complete. |callback| is added to a list of callbacks
  // and they are notified in the order they were received.
  // Use from the UI thread.
  void GetAndroidEnterpriseInfoState(EnterpriseInfoCallback callback);

 private:
  friend base::NoDestructor<AndroidEnterpriseInfo>;
  friend AndroidEnterpriseInfoFriendHelper;

  AndroidEnterpriseInfo();

  // This function is for the Java side code to return its result.
  // Calls are made on the UI thread.
  void ServiceCallbacks(bool profile_owned, bool device_owned);

  std::queue<EnterpriseInfoCallback> callback_queue_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(AndroidEnterpriseInfo);
};

}  // namespace enterprise_util
}  // namespace chrome

#endif  // CHROME_BROWSER_ENTERPRISE_UTIL_ANDROID_ENTERPRISE_INFO_H_
