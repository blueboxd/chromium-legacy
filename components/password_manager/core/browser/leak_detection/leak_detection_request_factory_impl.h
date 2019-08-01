// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_IMPL_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "url/gurl.h"

namespace password_manager {

// The class creates instances of requests for checking if {username, password}
// pair was leaked in the internet.
class LeakDetectionRequestFactoryImpl : public LeakDetectionRequestFactory {
 public:
  LeakDetectionRequestFactoryImpl();
  ~LeakDetectionRequestFactoryImpl() override;

  std::unique_ptr<LeakDetectionCheck> TryCreateLeakCheck() const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_LEAK_DETECTION_REQUEST_FACTORY_IMPL_H_
