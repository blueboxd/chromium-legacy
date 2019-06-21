// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_

#include "ash/public/interfaces/accessibility_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/events/event.h"

// SwitchAccessEventHandlerDelegate receives mouse and key events and forwards
// them to the Switch Access extension in Chrome. The SwitchAccessEventHandler
// in the Ash process handles events and passes them to this delegate via a Mojo
// interface defined in accessibility_controller.mojom.
class SwitchAccessEventHandlerDelegate
    : public ash::mojom::SwitchAccessEventHandlerDelegate {
 public:
  SwitchAccessEventHandlerDelegate();
  ~SwitchAccessEventHandlerDelegate() override;

 private:
  // ash::mojom::SwitchAccessEventHandlerDelegate:
  void DispatchKeyEvent(std::unique_ptr<ui::Event> event) override;
  void SendSwitchAccessCommand(
      ash::mojom::SwitchAccessCommand command) override;

  mojo::Binding<ash::mojom::SwitchAccessEventHandlerDelegate> binding_;

  DISALLOW_COPY_AND_ASSIGN(SwitchAccessEventHandlerDelegate);
};

#endif  // CHROME_BROWSER_CHROMEOS_ACCESSIBILITY_SWITCH_ACCESS_EVENT_HANDLER_DELEGATE_H_
