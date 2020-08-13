// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/extension/service.h"

#include "base/logging.h"
#include "chrome/credential_provider/extension/os_service_manager.h"
#include "chrome/credential_provider/gaiacp/logging.h"

namespace credential_provider {
namespace extension {

// static
Service** Service::GetInstanceStorage() {
  static Service* instance = new Service();

  return &instance;
}

// static
Service* Service::Get() {
  return *GetInstanceStorage();
}

DWORD Service::Run() {
  return (this->*run_routine_)();
}

Service::Service()
    : run_routine_(&Service::RunAsService),
      service_status_(),
      service_status_handle_(nullptr),
      stop_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                  base::WaitableEvent::InitialState::NOT_SIGNALED) {
  service_status_.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  service_status_.dwCurrentState = SERVICE_STOPPED;
  service_status_.dwControlsAccepted =
      SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PRESHUTDOWN;
}

Service::~Service() {}

DWORD Service::RunAsService() {
  LOGFN(INFO);

  DWORD error_code =
      extension::OSServiceManager::Get()->StartServiceCtrlDispatcher(
          &Service::ServiceMain);

  if (error_code != ERROR_SUCCESS) {
    LOGFN(ERROR)
        << "OSServiceManager::StartServiceCtrlDispatcher failed with win32="
        << error_code;
  }

  return error_code;
}

void Service::StartMain() {
  DWORD error_code = extension::OSServiceManager::Get()->RegisterCtrlHandler(
      &Service::ServiceControlHandler, &service_status_handle_);
  if (error_code != ERROR_SUCCESS) {
    LOGFN(ERROR) << "OSServiceManager::RegisterCtrlHandler failed win32="
                 << error_code;
    return;
  }

  service_status_.dwCurrentState = SERVICE_RUNNING;

  error_code = extension::OSServiceManager::Get()->SetServiceStatus(
      service_status_handle_, service_status_);
  if (error_code != ERROR_SUCCESS) {
    LOGFN(ERROR) << "OSServiceManager::SetServiceStatus failed win32="
                 << error_code;
    return;
  }

  stop_event_.Wait();

  service_status_.dwCurrentState = SERVICE_STOPPED;
  service_status_.dwControlsAccepted = 0;

  error_code = extension::OSServiceManager::Get()->SetServiceStatus(
      service_status_handle_, service_status_);
  if (error_code != ERROR_SUCCESS)
    LOGFN(ERROR) << "OSServiceManager::SetServiceStatus failed win32="
                 << error_code;
}

// static
VOID WINAPI Service::ServiceMain(DWORD argc /*unused*/,
                                 WCHAR* argv[] /*unused*/) {
  LOGFN(INFO);

  Service* self = Service::Get();

  // Run the service.
  self->StartMain();
}

// static
VOID WINAPI Service::ServiceControlHandler(DWORD control) {
  LOGFN(INFO);

  Service* self = Service::Get();
  switch (control) {
    case SERVICE_CONTROL_PRESHUTDOWN:
    case SERVICE_CONTROL_STOP:
      self->service_status_.dwCurrentState = SERVICE_STOP_PENDING;

      extension::OSServiceManager::Get()->SetServiceStatus(
          self->service_status_handle_, self->service_status_);
      self->stop_event_.Signal();

      break;
    default:
      break;
  }
}

}  // namespace extension
}  // namespace credential_provider
