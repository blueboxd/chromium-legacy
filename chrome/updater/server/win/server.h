// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_WIN_SERVER_H_
#define CHROME_UPDATER_SERVER_WIN_SERVER_H_

#include <windows.h>

#include <wrl/implements.h>
#include <wrl/module.h>
#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/updater/server/win/updater_idl.h"

namespace updater {

class UpdateService;

// This class implements the IUpdater interface and exposes it as a COM object.
class UpdaterImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdater> {
 public:
  UpdaterImpl() = default;

  IFACEMETHODIMP CheckForUpdate(const base::char16* guid) override;
  IFACEMETHODIMP Update(const base::char16* guid) override;

 private:
  ~UpdaterImpl() override = default;

  DISALLOW_COPY_AND_ASSIGN(UpdaterImpl);
};

// This class is responsible for the lifetime of the COM server, as well as
// class factory registration.
class ComServer {
 public:
  ComServer();
  ~ComServer() = default;

  // Main entry point for the COM server.
  int RunComServer();

 private:
  // Registers and unregisters the out-of-process COM class factories.
  HRESULT RegisterClassObject();
  void UnregisterClassObject();

  // Waits until the last COM object is released.
  void WaitForExitSignal();

  // Called when the last object is released.
  void SignalExit();

  // Creates an out-of-process WRL Module.
  void CreateWRLModule();

  // Handles object registration, message loop, and unregistration. Returns
  // when all registered objects are released.
  HRESULT Run();

  // Identifier of registered class objects used for unregistration.
  DWORD cookies_[1] = {};

  // This event is signaled when the last COM instance is released.
  base::WaitableEvent exit_signal_;

  DISALLOW_COPY_AND_ASSIGN(ComServer);
};

// Sets up and runs the server.
int RunServer(std::unique_ptr<UpdateService> update_service);

}  // namespace updater

#endif  // CHROME_UPDATER_SERVER_WIN_SERVER_H_
