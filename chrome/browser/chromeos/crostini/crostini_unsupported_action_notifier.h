// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_

#include <memory>

#include "ash/public/cpp/tablet_mode_observer.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/message_center/public/cpp/notification.h"

namespace crostini {

// Notifies the user when they try to do something Crostini doesn't yet support
// e.g. use the virtual keyboard in a crostini app.
// TODO(davidmunro): Emit metrics around how often we're hitting these issues so
// we can prioritise appropriately.
class CrostiniUnsupportedActionNotifier
    : public ash::TabletModeObserver,
      public aura::client::FocusChangeObserver {
 public:
  // Adapter around external integrations which we can mock out for testing,
  // stateless.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    virtual bool IsInTabletMode();

    // True if the window which currently has focus is a crostini window,
    // doesn't count the terminal.
    virtual bool IsFocusedWindowCrostini();

    // Shows a notification to the user, caller manages the lifecycle of the
    // notification.
    virtual void ShowNotification(message_center::Notification* notification);

    virtual void AddFocusObserver(aura::client::FocusChangeObserver* observer);
    virtual void RemoveFocusObserver(
        aura::client::FocusChangeObserver* observer);
    virtual void AddTabletModeObserver(ash::TabletModeObserver* observer);
    virtual void RemoveTabletModeObserver(ash::TabletModeObserver* observer);
  };

  CrostiniUnsupportedActionNotifier();
  explicit CrostiniUnsupportedActionNotifier(
      std::unique_ptr<Delegate> delegate);
  ~CrostiniUnsupportedActionNotifier() override;

  // Checks if the user is trying to use a virtual keyboard with a crostini
  // app and, if so and if they haven't already been notified that it's not
  // supported, notify them.
  void ShowVirtualKeyboardUnsupportedNotifictionIfNeeded();

  // ash::TabletModeObserver
  void OnTabletModeStarted() override;

  // aura::client::FocusChangeObserver
  void OnWindowFocused(aura::Window* gained_focus,
                       aura::Window* lost_focus) override;

  Delegate* get_delegate_for_testing() { return delegate_.get(); }

 private:
  std::unique_ptr<Delegate> delegate_;
  bool virtual_keyboard_unsupported_message_shown_ = false;

  DISALLOW_COPY_AND_ASSIGN(CrostiniUnsupportedActionNotifier);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UNSUPPORTED_ACTION_NOTIFIER_H_
