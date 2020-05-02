// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_

#include "base/run_loop.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "url/gurl.h"

namespace content {
class ServiceWorkerContext;
}

namespace extensions {
namespace service_worker_test_utils {

// An observer for service worker registration events.
class TestRegistrationObserver : public content::ServiceWorkerContextObserver {
 public:
  using RegistrationsMap = std::map<GURL, int>;

  explicit TestRegistrationObserver(content::ServiceWorkerContext* context);
  ~TestRegistrationObserver() override;

  TestRegistrationObserver(const TestRegistrationObserver&) = delete;
  TestRegistrationObserver& operator=(const TestRegistrationObserver&) = delete;

  // Wait for the first service worker registration with an extension scheme
  // scope to be stored.
  void WaitForRegistrationStored();

  // Returns the number of completed registrations for |scope|.
  int GetCompletedCount(const GURL& scope) const;

 private:
  // ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  RegistrationsMap registrations_completed_map_;
  base::RunLoop stored_run_loop_;
  content::ServiceWorkerContext* context_ = nullptr;
};

}  // namespace service_worker_test_utils
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_TEST_UTILS_H_
