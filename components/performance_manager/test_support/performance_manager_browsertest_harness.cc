// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/test_support/performance_manager_browsertest_harness.h"

#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
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

  // Set up the embedded web server.
  host_resolver()->AddRule("*", "127.0.0.1");
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "components/test/data/performance_manager");
  ASSERT_TRUE(embedded_test_server()->Start());
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

void PerformanceManagerBrowserTestHarness::StartNavigation(
    content::WebContents* contents,
    const GURL& url) {
  // See content/public/test/browser_test_utils.cc
  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  contents->GetController().LoadURLWithParams(params);
  contents->Focus();
}

namespace {

class WaitForLoadObserver : public content::WebContentsObserver {
 public:
  explicit WaitForLoadObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents) {}
  ~WaitForLoadObserver() override = default;

  void Wait() {
    if (!web_contents()->IsLoading())
      return;
    run_loop_.Run();
  }

 private:
  // WebContentsObserver implementation
  void DidStopLoading() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

}  // namespace

void PerformanceManagerBrowserTestHarness::WaitForLoad(
    content::WebContents* contents) {
  WaitForLoadObserver observer(contents);
  observer.Wait();
}

}  // namespace performance_manager
