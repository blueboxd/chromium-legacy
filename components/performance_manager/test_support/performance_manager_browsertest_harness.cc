// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"

#include "base/bind_helpers.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/service_manager/public/cpp/binder_registry.h"

namespace performance_manager {

PerformanceManagerBrowserTestHarness::PerformanceManagerBrowserTestHarness() {
  helper_ = std::make_unique<PerformanceManagerTestHarnessHelper>();
}

PerformanceManagerBrowserTestHarness::~PerformanceManagerBrowserTestHarness() =
    default;

void PerformanceManagerBrowserTestHarness::PreRunTestOnMainThread() {
  Super::PreRunTestOnMainThread();
  helper_->SetUp();
  helper_->OnWebContentsCreated(shell()->web_contents());
}

void PerformanceManagerBrowserTestHarness::PostRunTestOnMainThread() {
  helper_->TearDown();
  Super::PostRunTestOnMainThread();
}

void PerformanceManagerBrowserTestHarness::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Ensure the PM logic is enabled in renderers.
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "PerformanceManagerInstrumentation");
}

// We're a full embedder of the PM, so we have to wire up all of the embedder
// hooks.
void PerformanceManagerBrowserTestHarness::CreatedBrowserMainParts(
    content::BrowserMainParts* browser_main_parts) {
  helper_->SetUp();

  // Expose interfaces to RenderProcess.
  content::ShellContentBrowserClient::Get()
      ->set_expose_interfaces_to_renderer_callback(base::BindRepeating(
          [](service_manager::BinderRegistry* registry,
             blink::AssociatedInterfaceRegistry* associated_registry_unused,
             content::RenderProcessHost* render_process_host) {
            PerformanceManagerRegistry::GetInstance()
                ->CreateProcessNodeAndExposeInterfacesToRendererProcess(
                    registry, render_process_host);
          }));

  // Expose interfaces to RenderFrame.
  content::ShellContentBrowserClient::Get()
      ->set_register_browser_interface_binders_for_frame_callback(
          base::BindRepeating(
              [](content::RenderFrameHost* render_frame_host,
                 mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
                PerformanceManagerRegistry::GetInstance()
                    ->ExposeInterfacesToRenderFrame(map);
              }));
}

content::Shell* PerformanceManagerBrowserTestHarness::CreateShell() {
  content::Shell* shell = CreateBrowser();
  helper_->OnWebContentsCreated(shell->web_contents());
  return shell;
}

}  // namespace performance_manager
