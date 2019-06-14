// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/chrome_browser_main_extra_parts_performance_manager.h"

#include <utility>

#include "chrome/browser/performance_manager/browser_child_process_watcher.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "chrome/browser/performance_manager/performance_manager_tab_helper.h"
#include "chrome/browser/performance_manager/render_process_user_data.h"

ChromeBrowserMainExtraPartsPerformanceManager::
    ChromeBrowserMainExtraPartsPerformanceManager() = default;
ChromeBrowserMainExtraPartsPerformanceManager::
    ~ChromeBrowserMainExtraPartsPerformanceManager() = default;

void ChromeBrowserMainExtraPartsPerformanceManager::PostCreateThreads() {
  performance_manager_ = performance_manager::PerformanceManager::Create();
  browser_child_process_watcher_ =
      std::make_unique<performance_manager::BrowserChildProcessWatcher>();
}

void ChromeBrowserMainExtraPartsPerformanceManager::PostMainMessageLoopRun() {
  // Release all graph nodes before destroying the performance manager.
  // First release the browser and GPU process nodes.
  browser_child_process_watcher_.reset();

  // There may still be WebContents with attached tab helpers at this point in
  // time, and there's no convenient later call-out to destroy the performance
  // manager. To release the page and frame nodes, detach the tab helpers
  // from any existing WebContents.
  performance_manager::PerformanceManagerTabHelper::DetachAndDestroyAll();

  // Then the render process nodes. These have to be destroyed after the
  // frame nodes.
  performance_manager::RenderProcessUserData::DetachAndDestroyAll();

  performance_manager::PerformanceManager::Destroy(
      std::move(performance_manager_));
}
