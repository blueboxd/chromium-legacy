// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/spoken_feedback_event_rewriter_delegate.h"

#include "ash/public/cpp/event_rewriter_controller.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/event_handler_common.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "components/arc/arc_util.h"
#include "components/exo/wm_helper.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_host.h"
#include "extensions/common/constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/events/event.h"

SpokenFeedbackEventRewriterDelegate::SpokenFeedbackEventRewriterDelegate() {
  // If WMHelper doesn't exist, do nothing. This occurs in tests.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->AddActivationObserver(this);
}

SpokenFeedbackEventRewriterDelegate::~SpokenFeedbackEventRewriterDelegate() {
  // If WMHelper is already destroyed, do nothing.
  // TODO(crbug.com/748380): Fix shutdown order.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
}

void SpokenFeedbackEventRewriterDelegate::DispatchKeyEventToChromeVox(
    std::unique_ptr<ui::Event> event,
    bool capture) {
  extensions::ExtensionHost* host = chromeos::GetAccessibilityExtensionHost(
      extension_misc::kChromeVoxExtensionId);
  if (!host)
    return;

  // Listen for any unhandled keyboard events from ChromeVox's background page
  // when capturing keys to reinject.
  host->host_contents()->SetDelegate(capture ? this : nullptr);

  // Forward the event to ChromeVox's background page.
  chromeos::ForwardKeyToExtension(*(event->AsKeyEvent()), host);
}

void SpokenFeedbackEventRewriterDelegate::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (gained_active == lost_active)
    return;

  is_arc_window_active_ = arc::IsArcAppWindow(gained_active);
}

void SpokenFeedbackEventRewriterDelegate::DispatchMouseEventToChromeVox(
    std::unique_ptr<ui::Event> event) {
  if (is_arc_window_active_)
    return;

  if (event->type() == ui::ET_MOUSE_MOVED) {
    AutomationManagerAura::GetInstance()->HandleEvent(
        ax::mojom::Event::kMouseMoved);
  }
}

void SpokenFeedbackEventRewriterDelegate::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  ash::EventRewriterController::Get()->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

bool SpokenFeedbackEventRewriterDelegate::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  OnUnhandledSpokenFeedbackEvent(ui::Event::Clone(*event.os_event));
  return true;
}
