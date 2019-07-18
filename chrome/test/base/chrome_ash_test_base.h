// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_ASH_TEST_BASE_H_
#define CHROME_TEST_BASE_CHROME_ASH_TEST_BASE_H_

#include "ash/test/ash_test_base.h"
#include "content/public/test/test_browser_thread_bundle.h"

// AshTestBase used in Chrome.
// TODO(crbug.com/890677): Chrome should not have tests subclassing
// AshTestBase.
class ChromeAshTestBase : public ash::AshTestBase {
 public:
  ChromeAshTestBase();
  ~ChromeAshTestBase() override;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAshTestBase);
};

#endif  // CHROME_TEST_BASE_CHROME_ASH_TEST_BASE_H_
