// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_WIN_SERVER_H_
#define CHROME_UPDATER_SERVER_WIN_SERVER_H_

#include <wrl/implements.h>
#include <wrl/module.h>

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/update_service.h"

namespace updater {

class Configurator;
class UpdateService;

// Returns an application object bound to this COM server.
scoped_refptr<App> AppServerInstance();

// The COM objects involved in this server are free threaded. Incoming COM calls
// arrive on COM RPC threads. Outgoing COM calls are posted from a blocking
// sequenced task runner in the thread pool. Calls to the update service occur
// in the main sequence, which is bound to the main thread.
//
// The free-threaded COM objects exposed by this server are entered either by
// COM RPC threads, when their functions are invoked by COM clients, or by
// threads from the updater's thread pool, when callbacks posted by the
// update service are handled. Access to the shared state maintained by these
// objects is synchronized by a lock. The sequencing of callbacks is ensured
// by using a sequenced task runner, since the callbacks can't use base
// synchronization primitives on the main sequence where they are posted from.
//
// This class is responsible for the lifetime of the COM server, as well as
// class factory registration.
class ComServerApp : public App {
 public:
  ComServerApp();

  // Returns the singleton instance of this ComServerApp.
  static scoped_refptr<ComServerApp> Instance() {
    return static_cast<ComServerApp*>(AppServerInstance().get());
  }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() {
    return main_task_runner_;
  }
  scoped_refptr<UpdateService> service() { return service_; }

 private:
  ~ComServerApp() override;

  // Overrides for App.
  void InitializeThreadPool() override;
  void Initialize() override;
  void FirstTaskRun() override;

  // Registers and unregisters the out-of-process COM class factories.
  HRESULT RegisterClassObjects();
  void UnregisterClassObjects();

  // Waits until the last COM object is released.
  void WaitForExitSignal();

  // Called when the last object is released.
  void SignalExit();

  // Creates an out-of-process WRL Module.
  void CreateWRLModule();

  // Handles object unregistration then triggers program shutdown.
  void Stop();

  // Identifier of registered class objects used for unregistration.
  DWORD cookies_[2] = {};

  // While this object lives, COM can be used by all threads in the program.
  base::win::ScopedCOMInitializer com_initializer_;

  // Task runner bound to the main sequence and the update service instance.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  // The UpdateService to use for handling the incoming COM requests. This
  // instance of the service runs the in-process update service code, which is
  // delegating to the update_client component.
  scoped_refptr<UpdateService> service_;

  // The updater's Configurator.
  scoped_refptr<Configurator> config_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_SERVER_WIN_SERVER_H_
