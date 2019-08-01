// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_

#include <memory>

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

namespace password_manager {

class LeakDetectionCheck;

// The helper class that incapsulates the requests and their processing.
class LeakDetectionDelegate {
 public:
  LeakDetectionDelegate();
  ~LeakDetectionDelegate();

  // Not copyable or movable
  LeakDetectionDelegate(const LeakDetectionDelegate&) = delete;
  LeakDetectionDelegate& operator=(const LeakDetectionDelegate&) = delete;
  LeakDetectionDelegate(LeakDetectionDelegate&&) = delete;
  LeakDetectionDelegate& operator=(LeakDetectionDelegate&&) = delete;

#if defined(UNIT_TEST)
  void set_leak_factory(std::unique_ptr<LeakDetectionRequestFactory> factory) {
    leak_factory_ = std::move(factory);
  }
#endif  // defined(UNIT_TEST)

  void StartLeakCheck(const autofill::PasswordForm& form);

 private:
  // The factory that creates objects for performing a leak check up.
  std::unique_ptr<LeakDetectionRequestFactory> leak_factory_;

  // Current leak check-up being performed in the background.
  std::unique_ptr<LeakDetectionCheck> leak_check_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_DELEGATE_H_
