// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_

#include <string>

#include "ash/public/cpp/login_types.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/chrome_web_modal_dialog_manager_delegate.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "ui/display/display_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class TabletModeClient;

namespace content {
class WebContents;
}

namespace display {
class Screen;
}

namespace ui {
class Accelerator;
}

namespace views {
class WebDialogView;
class Widget;
}  // namespace views

namespace chromeos {

class LoginDisplayHostMojo;
class OobeUI;

class CaptivePortalDialogDelegate;

// This class manages the behavior of the Oobe UI dialog.
// And its lifecycle is managed by the widget created in Show().
//   WebDialogView<----delegate_----OobeUIDialogDelegate
//         |
//         |
//         V
//   clientView---->Widget's view hierarchy
class OobeUIDialogDelegate : public display::DisplayObserver,
                             public TabletModeClientObserver,
                             public ui::WebDialogDelegate,
                             public ChromeKeyboardControllerClient::Observer,
                             public CaptivePortalWindowProxy::Observer {
 public:
  explicit OobeUIDialogDelegate(base::WeakPtr<LoginDisplayHostMojo> controller);
  ~OobeUIDialogDelegate() override;

  // Show the dialog widget.
  void Show();

  // Show the dialog widget stretched to full screen.
  void ShowFullScreen();

  // Close the widget, and it will delete this object.
  void Close();

  // Hide the dialog widget, but do not shut it down.
  void Hide();

  // Returns whether the dialog is currently visible.
  bool IsVisible();

  // Update the oobe state of the dialog.
  void SetState(ash::OobeDialogState state);

  // Tell the dialog whether to call FixCaptivePortal next time it is shown.
  void SetShouldDisplayCaptivePortal(bool should_display);

  content::WebContents* GetWebContents();

  void UpdateSizeAndPosition(int width, int height);
  OobeUI* GetOobeUI() const;
  gfx::NativeWindow GetNativeWindow() const;

 private:
  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  // TabletModeClientObserver:
  void OnTabletModeToggled(bool enabled) override;

  // ui::WebDialogDelegate:
  ui::ModalType GetDialogModalType() const override;
  base::string16 GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  bool CanResizeDialog() const override;
  std::string GetDialogArgs() const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  std::vector<ui::Accelerator> GetAccelerators() override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // ChromeKeyboardControllerClient::Observer:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // CaptivePortalWindowProxy::Observer:
  void OnBeforeCaptivePortalShown() override;
  void OnAfterCaptivePortalHidden() override;

  base::WeakPtr<LoginDisplayHostMojo> controller_;

  base::WeakPtr<CaptivePortalDialogDelegate> captive_portal_delegate_;

  // This is owned by the underlying native widget.
  // Before its deletion, onDialogClosed will get called and delete this object.
  views::Widget* dialog_widget_ = nullptr;
  views::WebDialogView* dialog_view_ = nullptr;
  gfx::Size size_;
  bool showing_fullscreen_ = false;

  ScopedObserver<display::Screen, display::DisplayObserver> display_observer_{
      this};
  ScopedObserver<TabletModeClient, TabletModeClientObserver>
      tablet_mode_observer_{this};
  ScopedObserver<ChromeKeyboardControllerClient,
                 ChromeKeyboardControllerClient::Observer>
      keyboard_observer_{this};
  ScopedObserver<CaptivePortalWindowProxy, OobeUIDialogDelegate>
      captive_portal_observer_{this};

  std::map<ui::Accelerator, std::string> accel_map_;
  ash::OobeDialogState state_ = ash::OobeDialogState::HIDDEN;

  // Whether the captive portal screen should be shown the next time the Gaia
  // dialog is opened.
  bool should_display_captive_portal_ = false;

  DISALLOW_COPY_AND_ASSIGN(OobeUIDialogDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_UI_OOBE_UI_DIALOG_DELEGATE_H_
