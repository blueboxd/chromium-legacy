// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"

namespace performance_manager {
class BrowserChildProcessWatcher;
class PerformanceManager;
}  // namespace performance_manager

class ChromeBrowserMainExtraPartsPerformanceManager
    : public ChromeBrowserMainExtraParts {
 public:
  ChromeBrowserMainExtraPartsPerformanceManager();
  ~ChromeBrowserMainExtraPartsPerformanceManager() override;

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostCreateThreads() override;
  void PostMainMessageLoopRun() override;

  std::unique_ptr<performance_manager::PerformanceManager> performance_manager_;

  std::unique_ptr<performance_manager::BrowserChildProcessWatcher>
      browser_child_process_watcher_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsPerformanceManager);
};

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
