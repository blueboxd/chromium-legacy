// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DISABLED_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DISABLED_CHECKER_H_

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

class SyncDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncDisabledChecker(syncer::ProfileSyncService* service);

  ~SyncDisabledChecker() override;
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  base::OnceClosure condition_satisfied_callback_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_DISABLED_CHECKER_H_
