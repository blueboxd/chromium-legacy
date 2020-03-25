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
#include <wrl/implements.h>
#include <wrl/module.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/configurator.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/update_service_in_process.h"

namespace updater {

// This class is responsible for the lifetime of the COM server, as well as
// class factory registration.
class ComServer : public App {
 public:
  ComServer();

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

  // The UpdateService to use for handling COM requests.
  std::unique_ptr<UpdateService> service_;

  // The updater's Configurator.
  scoped_refptr<Configurator> config_;
};

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

HRESULT UpdaterImpl::UpdateAll(IUpdaterObserver* observer) {
  if (observer) {
    auto status = Microsoft::WRL::Make<CompleteStatusImpl>(11, L"Test");
    observer->OnComplete(status.Get());
  }

  return S_OK;
}

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
  service_ = std::make_unique<UpdateServiceInProcess>(config_);
  CreateWRLModule();
  HRESULT hr = RegisterClassObject();
  if (FAILED(hr))
    Shutdown(hr);
}

scoped_refptr<App> AppServerInstance() {
  return AppInstance<ComServer>();
}

}  // namespace updater
