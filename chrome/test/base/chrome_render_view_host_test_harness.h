// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
#define CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "content/public/test/test_renderer_host.h"

class TestingProfile;

// Wrapper around RenderViewHostTestHarness that uses a TestingProfile as
// browser context instead of a TestBrowserContext.
class ChromeRenderViewHostTestHarness
    : public content::RenderViewHostTestHarness {
 public:
  // Construct a ChromeRenderViewHostTestHarness with zero or more arguments
  // passed to content::RenderViewHostTestHarness.
  template <typename... Args>
  constexpr ChromeRenderViewHostTestHarness(Args... args)
      : content::RenderViewHostTestHarness(args...) {}

  ~ChromeRenderViewHostTestHarness() override;

  TestingProfile* profile();

 protected:
  // testing::Test
  void TearDown() override;

  // content::RenderViewHostTestHarness.
  content::BrowserContext* CreateBrowserContext() override;

 private:
  std::vector<std::unique_ptr<base::ScopedTempDir>> temp_dirs_;
};

#endif  // CHROME_TEST_BASE_CHROME_RENDER_VIEW_HOST_TEST_HARNESS_H_
