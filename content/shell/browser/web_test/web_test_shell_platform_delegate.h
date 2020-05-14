// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_SHELL_PLATFORM_DELEGATE_H_
#define CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_SHELL_PLATFORM_DELEGATE_H_

#include "build/build_config.h"
#include "content/shell/browser/shell_platform_delegate.h"

namespace content {

class WebTestShellPlatformDelegate : public ShellPlatformDelegate {
 public:
  WebTestShellPlatformDelegate();
  ~WebTestShellPlatformDelegate() override;

#if defined(OS_MACOSX)
  // ShellPlatformDelegate overrides.
  void CreatePlatformWindow(Shell* shell,
                            const gfx::Size& initial_size) override;
  gfx::NativeWindow GetNativeWindow(Shell* shell) override;
  void CleanUp(Shell* shell) override;
  void SetContents(Shell* shell) override;
  void EnableUIControl(Shell* shell,
                       UIControl control,
                       bool is_enabled) override;
  void SetAddressBarURL(Shell* shell, const GURL& url) override;
  void SetTitle(Shell* shell, const base::string16& title) override;
  void RenderViewReady(Shell* shell) override;
  bool DestroyShell(Shell* shell) override;
  void ResizeWebContent(Shell* shell, const gfx::Size& content_size) override;
  void ActivateContents(Shell* shell, WebContents* top_contents) override;
  bool HandleKeyboardEvent(Shell* shell,
                           WebContents* source,
                           const NativeWebKeyboardEvent& event) override;
#endif

 private:
#if defined(OS_MACOSX)
  // Data held for each Shell instance, since there is one ShellPlatformDelegate
  // for the whole browser process (shared across Shells). This is defined for
  // each platform implementation.
  struct WebTestShellData;

  // Holds an instance of WebTestShellData for each Shell.
  base::flat_map<Shell*, WebTestShellData> web_test_shell_data_map_;
#endif
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_WEB_TEST_WEB_TEST_SHELL_PLATFORM_DELEGATE_H_
