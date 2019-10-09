// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "components/performance_manager/process_node_source.h"
#include "components/performance_manager/tab_helper_frame_node_source.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace performance_manager {
class BrowserChildProcessWatcher;
class GraphImpl;
class PerformanceManagerImpl;
class SharedWorkerWatcher;
}  // namespace performance_manager

// Handles the initialization of the performance manager and a few dependent
// classes that create/manage graph nodes.
class ChromeBrowserMainExtraPartsPerformanceManager
    : public ChromeBrowserMainExtraParts,
      public content::NotificationObserver {
 public:
  ChromeBrowserMainExtraPartsPerformanceManager();
  ~ChromeBrowserMainExtraPartsPerformanceManager() override;

  static void CreateDefaultPoliciesAndDecorators(
      performance_manager::GraphImpl* graph);

 private:
  // ChromeBrowserMainExtraParts overrides.
  void PostCreateThreads() override;
  void PostMainMessageLoopRun() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Handlers for profile creation and destruction notifications.
  void CreateSharedWorkerWatcher(Profile* profile);
  void DeleteSharedWorkerWatcher(Profile* profile);

  std::unique_ptr<performance_manager::PerformanceManagerImpl>
      performance_manager_;

  std::unique_ptr<performance_manager::BrowserChildProcessWatcher>
      browser_child_process_watcher_;

  content::NotificationRegistrar notification_registrar_;

  // Needed by the worker watchers to access existing process nodes and frame
  // nodes.
  performance_manager::ProcessNodeSource process_node_source_;
  performance_manager::TabHelperFrameNodeSource frame_node_source_;

  // Observes the lifetime of shared workers.
  base::flat_map<Profile*,
                 std::unique_ptr<performance_manager::SharedWorkerWatcher>>
      shared_worker_watchers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserMainExtraPartsPerformanceManager);
};

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_CHROME_BROWSER_MAIN_EXTRA_PARTS_PERFORMANCE_MANAGER_H_
