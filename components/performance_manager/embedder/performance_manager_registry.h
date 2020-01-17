// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_REGISTRY_H_
#define COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_REGISTRY_H_

#include <memory>

namespace content {
class BrowserContext;
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace performance_manager {

// Allows tracking of WebContents, RenderProcessHosts and SharedWorkerInstances
// in the PerformanceManager.
//
// A process that embeds the PerformanceManager should create a single instance
// of this and notify it when WebContents, RenderProcessHosts or BrowserContexts
// are created.
//
// TearDown() must be called prior to destroying this object. This will schedule
// deletion of PageNodes, ProcessNodes and WorkerNodes retained by this
// registry, even if the associated WebContents, RenderProcessHosts and
// SharedWorkerInstances still exist.
//
// This class can only be accessed on the main thread.
class PerformanceManagerRegistry {
 public:
  virtual ~PerformanceManagerRegistry() = default;

  PerformanceManagerRegistry(const PerformanceManagerRegistry&) = delete;
  void operator=(const PerformanceManagerRegistry&) = delete;

  // Creates a PerformanceManagerRegistry instance.
  static std::unique_ptr<PerformanceManagerRegistry> Create();

  // Returns the only instance of PerformanceManagerRegistry living in this
  // process, or nullptr if there is none.
  static PerformanceManagerRegistry* GetInstance();

  // Must be invoked when a WebContents is created. Creates an associated
  // PageNode in the PerformanceManager, if it doesn't already exist.
  //
  // Note: As of December 2019, this is called by the constructor of
  // DevtoolsWindow on its main WebContents. It may be called again for the same
  // WebContents by TabHelpers::AttachTabHelpers() when Devtools is docked.
  // Hence the support for calling CreatePageNodeForWebContents() for a
  // WebContents that already has a PageNode.
  virtual void CreatePageNodeForWebContents(
      content::WebContents* web_contents) = 0;

  // Must be invoked when a RenderProcessHost is created. Creates an associated
  // ProcessNode in the PerformanceManager, if it doesn't already exist.
  virtual void CreateProcessNodeForRenderProcessHost(
      content::RenderProcessHost* render_process_host) = 0;

  // Must be invoked when a BrowserContext is added/removed.
  // Registers/unregisters an observer that creates WorkerNodes when
  // SharedWorkerInstances are added in the BrowserContext.
  virtual void NotifyBrowserContextAdded(
      content::BrowserContext* browser_context) = 0;
  virtual void NotifyBrowserContextRemoved(
      content::BrowserContext* browser_context) = 0;

  // Must be invoked prior to destroying the object. Schedules deletion of
  // PageNodes and ProcessNodes retained by this registry, even if the
  // associated WebContents and RenderProcessHosts still exist.
  virtual void TearDown() = 0;

 protected:
  PerformanceManagerRegistry() = default;
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_EMBEDDER_PERFORMANCE_MANAGER_REGISTRY_H_
