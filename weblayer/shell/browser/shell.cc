// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/browser/shell.h"

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
#include "weblayer/public/browser_controller.h"
#include "weblayer/public/navigation_controller.h"

namespace weblayer {

// Null until/unless the default main message loop is running.
base::NoDestructor<base::OnceClosure> g_quit_main_message_loop;

const int kDefaultTestWindowWidthDip = 1000;
const int kDefaultTestWindowHeightDip = 700;

std::vector<Shell*> Shell::windows_;

Shell::Shell(std::unique_ptr<BrowserController> browser_controller)
    : browser_controller_(std::move(browser_controller)), window_(nullptr) {
  windows_.push_back(this);
  browser_controller_->AddObserver(this);
}

Shell::~Shell() {
  browser_controller_->RemoveObserver(this);
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
  browser_controller_.reset();

  if (windows_.empty()) {
    if (*g_quit_main_message_loop)
      std::move(*g_quit_main_message_loop).Run();
  }
}

Shell* Shell::CreateShell(std::unique_ptr<BrowserController> browser_controller,
                          const gfx::Size& initial_size) {
  Shell* shell = new Shell(std::move(browser_controller));
  shell->PlatformCreateWindow(initial_size.width(), initial_size.height());

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();

  return shell;
}

void Shell::CloseAllWindows() {
  std::vector<Shell*> open_windows(windows_);
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

void Shell::SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) {
  *g_quit_main_message_loop = std::move(quit_closure);
}

void Shell::Initialize() {
  PlatformInitialize(GetShellDefaultSize());
}

void Shell::LoadingStateChanged(bool is_loading, bool to_different_document) {
  int current_index = browser_controller_->GetNavigationController()
                          ->GetNavigationListCurrentIndex();
  int max_index =
      browser_controller_->GetNavigationController()->GetNavigationListSize() -
      1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON, to_different_document && is_loading);
}

void Shell::DisplayedURLChanged(const GURL& url) {
  PlatformSetAddressBarURL(url);
}

gfx::Size Shell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return GetShellDefaultSize();
}

Shell* Shell::CreateNewWindow(weblayer::Profile* web_profile,
                              const GURL& url,
                              const gfx::Size& initial_size) {
  auto browser_controller = BrowserController::Create(web_profile);

  Shell* shell = CreateShell(std::move(browser_controller),
                             AdjustWindowSize(initial_size));
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  browser_controller_->GetNavigationController()->Navigate(url);
}

void Shell::GoBackOrForward(int offset) {
  if (offset == -1)
    browser_controller_->GetNavigationController()->GoBack();
  else if (offset == 1)
    browser_controller_->GetNavigationController()->GoForward();
}

void Shell::Reload() {
  browser_controller_->GetNavigationController()->Reload();
}

void Shell::ReloadBypassingCache() {}

void Shell::Stop() {
  browser_controller_->GetNavigationController()->Stop();
}

gfx::Size Shell::GetShellDefaultSize() {
  static gfx::Size default_shell_size;
  if (!default_shell_size.IsEmpty())
    return default_shell_size;
  default_shell_size =
      gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
  return default_shell_size;
}

}  // namespace weblayer
