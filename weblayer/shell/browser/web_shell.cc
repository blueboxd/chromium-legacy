// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/browser/web_shell.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "url/gurl.h"
#include "weblayer/public/web_browser_controller.h"
#include "weblayer/public/web_navigation_controller.h"

namespace weblayer {

// Null until/unless the default main message loop is running.
base::NoDestructor<base::OnceClosure> g_quit_main_message_loop;

const int kDefaultTestWindowWidthDip = 800;
const int kDefaultTestWindowHeightDip = 600;

std::vector<WebShell*> WebShell::windows_;

WebShell::WebShell(std::unique_ptr<WebBrowserController> web_browser_controller)
    : web_browser_controller_(std::move(web_browser_controller)),
      window_(nullptr) {
  windows_.push_back(this);
}

WebShell::~WebShell() {
  PlatformCleanUp();

  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i] == this) {
      windows_.erase(windows_.begin() + i);
      break;
    }
  }

  // Always destroy WebContents before calling PlatformExit(). WebContents
  // destruction sequence may depend on the resources destroyed in
  // PlatformExit() (e.g. the display::Screen singleton).
  web_browser_controller_.reset();

  if (windows_.empty()) {
    if (*g_quit_main_message_loop)
      std::move(*g_quit_main_message_loop).Run();
  }
}

WebShell* WebShell::CreateWebShell(
    std::unique_ptr<WebBrowserController> web_browser_controller,
    const gfx::Size& initial_size) {
  WebShell* shell = new WebShell(std::move(web_browser_controller));
  shell->PlatformCreateWindow(initial_size.width(), initial_size.height());

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();

  return shell;
}

void WebShell::CloseAllWindows() {
  std::vector<WebShell*> open_windows(windows_);
  for (size_t i = 0; i < open_windows.size(); ++i)
    open_windows[i]->Close();

  // Pump the message loop to allow window teardown tasks to run.
  base::RunLoop().RunUntilIdle();

  // If there were no windows open then the message loop quit closure will
  // not have been run.
  if (*g_quit_main_message_loop)
    std::move(*g_quit_main_message_loop).Run();

  PlatformExit();
}

void WebShell::SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) {
  *g_quit_main_message_loop = std::move(quit_closure);
}

void WebShell::Initialize() {
  PlatformInitialize(GetShellDefaultSize());
}

gfx::Size WebShell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return GetShellDefaultSize();
}

WebShell* WebShell::CreateNewWindow(weblayer::WebProfile* web_profile,
                                    const GURL& url,
                                    const gfx::Size& initial_size) {
  auto adjusted_size = AdjustWindowSize(initial_size);
  auto web_browser_controller =
      WebBrowserController::Create(web_profile, adjusted_size);

  WebShell* shell =
      CreateWebShell(std::move(web_browser_controller), adjusted_size);
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void WebShell::LoadURL(const GURL& url) {
  web_browser_controller_->GetWebNavigationController()->Navigate(url);
}

void WebShell::GoBackOrForward(int offset) {
  if (offset == -1)
    web_browser_controller_->GetWebNavigationController()->GoBack();
  else if (offset == 1)
    web_browser_controller_->GetWebNavigationController()->GoForward();
}

void WebShell::Reload() {
  web_browser_controller_->GetWebNavigationController()->Reload();
}

void WebShell::ReloadBypassingCache() {}

void WebShell::Stop() {
  web_browser_controller_->GetWebNavigationController()->Stop();
}

/* TODO: this depends on getting notifications from WebBrowserController.
void WebShell::UpdateNavigationControls(bool to_different_document) {
  int current_index = web_contents_->GetController().GetCurrentEntryIndex();
  int max_index = web_contents_->GetController().GetEntryCount() - 1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON,
      to_different_document && web_contents_->IsLoading());
}
*/

gfx::Size WebShell::GetShellDefaultSize() {
  static gfx::Size default_shell_size;
  if (!default_shell_size.IsEmpty())
    return default_shell_size;
  default_shell_size =
      gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
  return default_shell_size;
}

}  // namespace weblayer
