// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_views_test_base.h"

#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "content/public/test/browser_task_environment.h"

ChromeViewsTestBase::ChromeViewsTestBase()
    : views::ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
          std::make_unique<content::BrowserTaskEnvironment>(
              content::BrowserTaskEnvironment::MainThreadType::UI,
              content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}

ChromeViewsTestBase::~ChromeViewsTestBase() = default;

void ChromeViewsTestBase::SetUp() {
  views::ViewsTestBase::SetUp();

  // This is similar to calling set_test_views_delegate() with a
  // ChromeTestViewsDelegate before the superclass SetUp(); however, this allows
  // the framework to provide whatever TestViewsDelegate subclass it likes as a
  // base.
  test_views_delegate()->set_layout_provider(
      ChromeLayoutProvider::CreateLayoutProvider());
}
