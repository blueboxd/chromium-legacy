// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT is not being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/updater/server/win/server.h"

#include <windows.h>

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {
namespace {

// The COM objects involved in this server are free threaded. Incoming COM calls
// arrive on COM RPC threads. Outgoing COM calls originating in the server are
// posted on blocking worker threads in the thread pool. Calls to the update
// service and update_client calls occur in the main sequence on the main
// thread.
//
// This class is responsible for the lifetime of the COM server, as well as
// class factory registration.
class ComServer : public App {
 public:
  ComServer();

  // Returns the singleton instance of this ComServer.
  static scoped_refptr<ComServer> Instance() {
    return static_cast<ComServer*>(AppServerInstance().get());
  }

  scoped_refptr<base::SequencedTaskRunner> main_task_runner() {
    return main_task_runner_;
  }
  scoped_refptr<UpdateService> service() { return service_; }

 private:
  ~ComServer() override = default;

  // Overrides for App.
  void InitializeThreadPool() override;
  void Initialize() override;
  void FirstTaskRun() override;

  // Registers and unregisters the out-of-process COM class factories.
  HRESULT RegisterClassObject();
  void UnregisterClassObject();

  // Waits until the last COM object is released.
  void WaitForExitSignal();

  // Called when the last object is released.
  void SignalExit();

  // Creates an out-of-process WRL Module.
  void CreateWRLModule();

  // Handles object unregistration then triggers program shutdown.
  void Stop();

  // Identifier of registered class objects used for unregistration.
  DWORD cookies_[1] = {};

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

ComServer::ComServer()
    : com_initializer_(base::win::ScopedCOMInitializer::kMTA) {}

void ComServer::InitializeThreadPool() {
  base::ThreadPoolInstance::Create(kThreadPoolName);

  // Reuses the logic in base::ThreadPoolInstance::StartWithDefaultParams.
  const int num_cores = base::SysInfo::NumberOfProcessors();
  const int max_num_foreground_threads = std::max(3, num_cores - 1);
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
  init_params.common_thread_pool_environment = base::ThreadPoolInstance::
      InitParams::CommonThreadPoolEnvironment::COM_MTA;
  base::ThreadPoolInstance::Get()->Start(init_params);
}

HRESULT ComServer::RegisterClassObject() {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();

  Microsoft::WRL::ComPtr<IUnknown> factory;
  unsigned int flags = Microsoft::WRL::ModuleType::OutOfProc;

  HRESULT hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<UpdaterImpl>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Factory creation failed; hr: " << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory;
  hr = factory.As(&class_factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }

  // The pointer in this array is unowned. Do not release it.
  IClassFactory* class_factories[] = {class_factory.Get()};
  static_assert(
      std::extent<decltype(cookies_)>() == base::size(class_factories),
      "Arrays cookies_ and class_factories must be the same size.");

  IID class_ids[] = {__uuidof(UpdaterClass)};
  DCHECK_EQ(base::size(cookies_), base::size(class_ids));
  static_assert(std::extent<decltype(cookies_)>() == base::size(class_ids),
                "Arrays cookies_ and class_ids must be the same size.");

  hr = module.RegisterCOMObject(nullptr, class_ids, class_factories, cookies_,
                                base::size(cookies_));
  if (FAILED(hr)) {
    LOG(ERROR) << "RegisterCOMObject failed; hr: " << hr;
    return hr;
  }

  return hr;
}

void ComServer::UnregisterClassObject() {
  auto& module = Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::GetModule();
  const HRESULT hr =
      module.UnregisterCOMObject(nullptr, cookies_, base::size(cookies_));
  if (FAILED(hr))
    LOG(ERROR) << "UnregisterCOMObject failed; hr: " << hr;
}

void ComServer::CreateWRLModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(this,
                                                            &ComServer::Stop);
}

void ComServer::Stop() {
  VLOG(2) << __func__ << ": COM server is shutting down.";
  UnregisterClassObject();
  Shutdown(0);
}

void ComServer::Initialize() {
  config_ = base::MakeRefCounted<Configurator>();
}

void ComServer::FirstTaskRun() {
  if (!com_initializer_.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    Shutdown(-1);
    return;
  }
  main_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  service_ = base::MakeRefCounted<UpdateServiceInProcess>(config_);
  CreateWRLModule();
  HRESULT hr = RegisterClassObject();
  if (FAILED(hr))
    Shutdown(hr);
}

}  // namespace

STDMETHODIMP CompleteStatusImpl::get_statusCode(LONG* code) {
  DCHECK(code);

  *code = code_;
  return S_OK;
}

STDMETHODIMP CompleteStatusImpl::get_statusMessage(BSTR* message) {
  DCHECK(message);

  *message = base::win::ScopedBstr(message_).Release();
  return S_OK;
}

HRESULT UpdaterImpl::CheckForUpdate(const base::char16* app_id) {
  return E_NOTIMPL;
}

HRESULT UpdaterImpl::Register(const base::char16* app_id,
                              const base::char16* brand_code,
                              const base::char16* tag,
                              const base::char16* version,
                              const base::char16* existence_checker_path) {
  return E_NOTIMPL;
}

HRESULT UpdaterImpl::Update(const base::char16* app_id) {
  return E_NOTIMPL;
}

// Called by the COM RPC runtime on one of its threads.
HRESULT UpdaterImpl::UpdateAll(IUpdaterObserver* observer) {
  using IUpdaterObserverPtr = Microsoft::WRL::ComPtr<IUpdaterObserver>;

  // Invoke the in-process |update_service| on the main sequence.
  auto com_server = ComServer::Instance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             IUpdaterObserverPtr observer) {
            update_service->UpdateAll(
                base::DoNothing(),
                base::BindOnce(
                    [](IUpdaterObserverPtr observer,
                       UpdateService::Result result) {
                      // The COM RPC outgoing call blocks and it must be posted
                      // through the thread pool.
                      // TODO(sorin) investigate if base::Bind can be fixed to
                      // bind stdcall COM invocations on the x86 architecture.
                      base::ThreadPool::PostTaskAndReplyWithResult(
                          FROM_HERE, {base::MayBlock()},
                          base::BindOnce(
                              &IUpdaterObserver::OnComplete, observer,
                              Microsoft::WRL::Make<CompleteStatusImpl>(
                                  static_cast<int>(result), L"Test")),
                          base::BindOnce([](HRESULT hr) {
                            VLOG(2) << "IUpdaterObserver::OnComplete returned "
                                    << std::hex << hr;
                          }));
                    },
                    observer));
          },
          com_server->service(), IUpdaterObserverPtr(observer)));

  return S_OK;
}

scoped_refptr<App> AppServerInstance() {
  return AppInstance<ComServer>();
}

}  // namespace updater
