// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_H_

#include <memory>

namespace password_manager {

class LeakDetectionCheck;

// The interface for creating instances of requests for checking if
// {username, password} pair was leaked in the internet.
class LeakDetectionRequestFactory {
 public:
  LeakDetectionRequestFactory() = default;
  virtual ~LeakDetectionRequestFactory() = default;

  // Not copyable or movable
  LeakDetectionRequestFactory(const LeakDetectionRequestFactory&) = delete;
  LeakDetectionRequestFactory& operator=(const LeakDetectionRequestFactory&) =
      delete;
  LeakDetectionRequestFactory(LeakDetectionRequestFactory&&) = delete;
  LeakDetectionRequestFactory& operator=(LeakDetectionRequestFactory&&) =
      delete;

  // The leak check is available only for signed-in users and if the feature is
  // available.
  virtual std::unique_ptr<LeakDetectionCheck> TryCreateLeakCheck() const = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_H_
