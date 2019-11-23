// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT is not being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/updater/win/com/com_server.h"

#include "base/logging.h"
#include "base/win/scoped_com_initializer.h"

namespace updater {

STDMETHODIMP UpdaterImpl::CheckForUpdate(const base::char16* guid) {
  return E_NOTIMPL;
}

STDMETHODIMP UpdaterImpl::Update(const base::char16* guid) {
  return E_NOTIMPL;
}

ComServer::ComServer()
    : exit_signal_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}

int ComServer::RunComServer() {
  // Initialize COM for the current thread.
  base::win::ScopedCOMInitializer com_initializer(
      base::win::ScopedCOMInitializer::kMTA);
  if (!com_initializer.Succeeded()) {
    PLOG(ERROR) << "Failed to initialize COM";
    return -1;
  }

  // Run the COM server.
  return Run();
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

void ComServer::WaitForExitSignal() {
  exit_signal_.Wait();
}

void ComServer::SignalExit() {
  exit_signal_.Signal();
}

void ComServer::CreateWRLModule() {
  Microsoft::WRL::Module<Microsoft::WRL::OutOfProc>::Create(
      this, &ComServer::SignalExit);
}

HRESULT ComServer::Run() {
  CreateWRLModule();
  HRESULT hr = RegisterClassObject();
  if (SUCCEEDED(hr)) {
    WaitForExitSignal();
    UnregisterClassObject();
  }

  return hr;
}

}  // namespace updater
