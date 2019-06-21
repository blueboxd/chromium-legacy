// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/switch_access_event_handler.h"

#include <set>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"

namespace ash {

namespace {

bool IsSwitchAccessEnabled() {
  return Shell::Get()->accessibility_controller()->switch_access_enabled();
}

void CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace

SwitchAccessEventHandler::SwitchAccessEventHandler(
    mojom::SwitchAccessEventHandlerDelegatePtr delegate_ptr)
    : delegate_ptr_(std::move(delegate_ptr)) {
  DCHECK(delegate_ptr_.is_bound());
  Shell::Get()->AddPreTargetHandler(this,
                                    ui::EventTarget::Priority::kAccessibility);
}

SwitchAccessEventHandler::~SwitchAccessEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void SwitchAccessEventHandler::FlushMojoForTest() {
  delegate_ptr_.FlushForTesting();
}

bool SwitchAccessEventHandler::SetKeyCodesForCommand(
    std::set<int> new_key_codes,
    mojom::SwitchAccessCommand command) {
  bool has_changed = false;
  std::set<int> to_clear;

  // Clear old values that conflict with the new assignment.
  for (const auto& val : command_for_key_code_) {
    int old_key_code = val.first;
    mojom::SwitchAccessCommand old_command = val.second;

    if (new_key_codes.count(old_key_code) > 0) {
      if (old_command != command) {
        has_changed = true;
        // Modifying the map while iterating through it causes reference
        // failures.
        to_clear.insert(old_key_code);
      } else {
        new_key_codes.erase(old_key_code);
      }
      continue;
    }

    // This value was previously mapped to the command, but is no longer.
    if (old_command == command) {
      has_changed = true;
      to_clear.insert(old_key_code);
      key_codes_to_capture_.erase(old_key_code);
    }
  }
  for (int key_code : to_clear) {
    command_for_key_code_.erase(key_code);
  }

  if (new_key_codes.size() == 0)
    return has_changed;

  // Add any new key codes to the map.
  for (int key_code : new_key_codes) {
    key_codes_to_capture_.insert(key_code);
    command_for_key_code_[key_code] = command;
  }

  return true;
}

void SwitchAccessEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(IsSwitchAccessEnabled());
  DCHECK(event);

  if (ShouldForwardEvent(*event)) {
    CancelEvent(event);
    // TODO(anastasi): Remove event dispatch once settings migration is
    // complete.
    delegate_ptr_->DispatchKeyEvent(ui::Event::Clone(*event));

    // The Command events are currently sent, but ignored. The next step of the
    // migration will be to switch from using the key events to using the
    // commands, and the final step will be to remove the key events entirely.
    mojom::SwitchAccessCommand command =
        command_for_key_code_[event->key_code()];
    delegate_ptr_->SendSwitchAccessCommand(command);
  }
}

bool SwitchAccessEventHandler::ShouldForwardEvent(
    const ui::KeyEvent& event) const {
  // Ignore virtual key events so users can type with the onscreen keyboard.
  if (ignore_virtual_key_events_ && !event.HasNativeEvent())
    return false;

  if (forward_key_events_)
    return true;

  return keys_to_capture_.find(event.key_code()) != keys_to_capture_.end();
}

}  // namespace ash
