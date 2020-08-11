// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_H_

#include <memory>

#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class FamilyUserSessionMetrics;

// Service to initialize and control metric recorders of family users on Chrome
// OS.
class FamilyUserMetricsService : public KeyedService {
 public:
  explicit FamilyUserMetricsService(content::BrowserContext* context);
  FamilyUserMetricsService(const FamilyUserMetricsService&) = delete;
  FamilyUserMetricsService& operator=(const FamilyUserMetricsService&) = delete;
  ~FamilyUserMetricsService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  std::unique_ptr<FamilyUserSessionMetrics> family_user_session_metrics_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_H_
