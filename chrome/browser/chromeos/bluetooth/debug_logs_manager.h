// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_DEBUG_LOGS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_DEBUG_LOGS_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace user_manager {
class User;
}  // namespace user_manager

// Manages the use of debug Bluetooth logs. Under normal usage, only warning and
// error logs are captured, but there are some situations in which it is
// advantageous to capture more verbose logs (e.g., in noisy environments or on
// a device with a particular Bluetooth chip). This class tracks the current
// state of debug logs and handles the user enabling/disabling them.
class DebugLogsManager : public mojom::DebugLogsChangeHandler {
 public:
  explicit DebugLogsManager(const user_manager::User* primary_user);
  ~DebugLogsManager() override;

  // State for capturing debug Bluetooth logs; logs are only captured when
  // supported and enabled. Debug logs are supported when the associated flag is
  // enabled and if an eligible user is signed in. Debug logs are enabled only
  // via interaction with the DebugLogsChangeHandler Mojo interface.
  enum class DebugLogsState {
    kNotSupported,
    kSupportedButDisabled,
    kSupportedAndEnabled
  };

  DebugLogsState GetDebugLogsState() const;

  // Generates an InterfacePtr bound to this object.
  mojom::DebugLogsChangeHandlerPtr GenerateInterfacePtr();

 private:
  // mojom::DebugLogsManager:
  void ChangeDebugLogsState(bool should_debug_logs_be_enabled) override;

  bool AreDebugLogsSupported() const;

  const user_manager::User* primary_user_ = nullptr;
  bool are_debug_logs_enabled_ = false;
  mojo::BindingSet<mojom::DebugLogsChangeHandler> bindings_;

  DISALLOW_COPY_AND_ASSIGN(DebugLogsManager);
};

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_DEBUG_LOGS_MANAGER_H_
