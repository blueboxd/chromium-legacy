// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/web_test_shell_platform_delegate.h"

#import "base/mac/foundation_util.h"
#include "content/browser/renderer_host/render_widget_host_view_mac.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/shell/browser/shell.h"

namespace content {

// On mac, the WebTestShellPlatformDelegate replaces behaviour in the base class
// ShellPlatformDelegate when in headless mode. Otherwise it defers to
// the base class.
// TODO(danakj): Maybe we should only instatiate a WebTestShellPlatformDelegate
// when in headless mode? Depends on the needs of other platforms.

struct WebTestShellPlatformDelegate::WebTestShellData {
  gfx::Size initial_size;
};

WebTestShellPlatformDelegate::WebTestShellPlatformDelegate() = default;
WebTestShellPlatformDelegate::~WebTestShellPlatformDelegate() = default;

void WebTestShellPlatformDelegate::CreatePlatformWindow(
    Shell* shell,
    const gfx::Size& initial_size) {
  if (!shell->headless()) {
    ShellPlatformDelegate::CreatePlatformWindow(shell, initial_size);
    return;
  }

  DCHECK(!base::Contains(web_test_shell_data_map_, shell));
  WebTestShellData& shell_data = web_test_shell_data_map_[shell];

  shell_data.initial_size = initial_size;
}

gfx::NativeWindow WebTestShellPlatformDelegate::GetNativeWindow(Shell* shell) {
  if (!shell->headless())
    return ShellPlatformDelegate::GetNativeWindow(shell);

  NOTREACHED();
  return {};
}

void WebTestShellPlatformDelegate::CleanUp(Shell* shell) {
  if (!shell->headless()) {
    ShellPlatformDelegate::GetNativeWindow(shell);
    return;
  }

  DCHECK(base::Contains(web_test_shell_data_map_, shell));
  web_test_shell_data_map_.erase(shell);
}

void WebTestShellPlatformDelegate::SetContents(Shell* shell) {
  if (!shell->headless()) {
    ShellPlatformDelegate::SetContents(shell);
    return;
  }
}

void WebTestShellPlatformDelegate::EnableUIControl(Shell* shell,
                                                   UIControl control,
                                                   bool is_enabled) {
  if (!shell->headless()) {
    ShellPlatformDelegate::EnableUIControl(shell, control, is_enabled);
    return;
  }
}

void WebTestShellPlatformDelegate::SetAddressBarURL(Shell* shell,
                                                    const GURL& url) {
  if (!shell->headless()) {
    ShellPlatformDelegate::SetAddressBarURL(shell, url);
    return;
  }
}

void WebTestShellPlatformDelegate::SetTitle(Shell* shell,
                                            const base::string16& title) {
  if (!shell->headless()) {
    ShellPlatformDelegate::SetTitle(shell, title);
    return;
  }
}

void WebTestShellPlatformDelegate::RenderViewReady(Shell* shell) {
  if (!shell->headless()) {
    ShellPlatformDelegate::RenderViewReady(shell);
    return;
  }

  DCHECK(base::Contains(web_test_shell_data_map_, shell));
  WebTestShellData& shell_data = web_test_shell_data_map_[shell];

  // In mac headless mode, the OS view for the WebContents is not attached to a
  // window so the usual notifications from the OS about the bounds of the web
  // contents do not occur. We need to make sure the renderer knows its bounds,
  // and to do this we force a resize to happen on the WebContents. However, the
  // WebContents can not be fully resized until after the RenderWidgetHostView
  // is created, which may not be not done until the first navigation starts.
  // Failing to do this resize *after* the navigation causes the
  // RenderWidgetHostView to be created would leave the WebContents with invalid
  // sizes (such as the window screen rect).
  //
  // We use the signal that the RenderView has been created in the renderer as
  // a proxy for knowing when the top level RenderWidgetHostView is created,
  // since they are created at the same time.
  DCHECK(shell->web_contents()->GetMainFrame()->GetView());
  ResizeWebContent(shell, shell_data.initial_size);
}

bool WebTestShellPlatformDelegate::DestroyShell(Shell* shell) {
  if (shell->headless())
    return false;  // Shell destroys itself.
  return ShellPlatformDelegate::DestroyShell(shell);
}

void WebTestShellPlatformDelegate::ResizeWebContent(
    Shell* shell,
    const gfx::Size& content_size) {
  if (!shell->headless()) {
    ShellPlatformDelegate::ResizeWebContent(shell, content_size);
    return;
  }

  NSView* web_view = shell->web_contents()->GetNativeView().GetNativeNSView();
  NSRect frame = NSMakeRect(0, 0, content_size.width(), content_size.height());
  [web_view setFrame:frame];

  // The above code changes the RenderWidgetHostView's size, but does not change
  // the widget's screen rects, since the RenerWidgetHostView is not attached to
  // a window in headless mode. So this call causes them to be updated so they
  // are not left as 0x0.
  auto* rwhv_mac = static_cast<RenderWidgetHostViewMac*>(
      shell->web_contents()->GetMainFrame()->GetView());
  if (rwhv_mac)
    rwhv_mac->OnWindowFrameInScreenChanged(gfx::Rect(content_size));
}

void WebTestShellPlatformDelegate::ActivateContents(Shell* shell,
                                                    WebContents* top_contents) {
  if (!shell->headless()) {
    ShellPlatformDelegate::ActivateContents(shell, top_contents);
    return;
  }

  // In headless mode, there are no system windows, so we can't go down the
  // normal path which relies on calling the OS to move focus/active states.
  // Instead we fake it out by just informing the RenderWidgetHost directly.

  // For all windows other than this one, blur them.
  for (Shell* window : Shell::windows()) {
    if (window != shell) {
      WebContents* other_top_contents = window->web_contents();
      RenderWidgetHost* other_main_widget =
          other_top_contents->GetMainFrame()->GetView()->GetRenderWidgetHost();
      other_main_widget->Blur();
      other_main_widget->SetActive(false);
    }
  }

  RenderWidgetHost* main_widget =
      top_contents->GetMainFrame()->GetView()->GetRenderWidgetHost();
  main_widget->Focus();
  main_widget->SetActive(true);
}

bool WebTestShellPlatformDelegate::HandleKeyboardEvent(
    Shell* shell,
    WebContents* source,
    const NativeWebKeyboardEvent& event) {
  if (shell->headless())
    return false;
  return ShellPlatformDelegate::HandleKeyboardEvent(shell, source, event);
}

}  // namespace content
