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

HRESULT ComServer::RegisterClassObjects() {
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

  Microsoft::WRL::ComPtr<IClassFactory> class_factory_updater;
  hr = factory.As(&class_factory_updater);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }
  factory.Reset();

  hr = Microsoft::WRL::Details::CreateClassFactory<
      Microsoft::WRL::SimpleClassFactory<LegacyOnDemandImpl>>(
      &flags, nullptr, __uuidof(IClassFactory), &factory);
  if (FAILED(hr)) {
    LOG(ERROR) << "Factory creation failed; hr: " << hr;
    return hr;
  }

  Microsoft::WRL::ComPtr<IClassFactory> class_factory_legacy_ondemand;
  hr = factory.As(&class_factory_legacy_ondemand);
  if (FAILED(hr)) {
    LOG(ERROR) << "IClassFactory object creation failed; hr: " << hr;
    return hr;
  }

  // The pointer in this array is unowned. Do not release it.
  IClassFactory* class_factories[] = {class_factory_updater.Get(),
                                      class_factory_legacy_ondemand.Get()};
  static_assert(
      std::extent<decltype(cookies_)>() == base::size(class_factories),
      "Arrays cookies_ and class_factories must be the same size.");

  IID class_ids[] = {__uuidof(UpdaterClass),
                     __uuidof(GoogleUpdate3WebUserClass)};
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

void ComServer::UnregisterClassObjects() {
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
  UnregisterClassObjects();
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
  HRESULT hr = RegisterClassObjects();
  if (FAILED(hr))
    Shutdown(hr);
}

}  // namespace

STDMETHODIMP LegacyOnDemandImpl::createAppBundleWeb(
    IDispatch** app_bundle_web) {
  DCHECK(app_bundle_web);

  Microsoft::WRL::ComPtr<IAppBundleWeb> app_bundle(this);
  *app_bundle_web = app_bundle.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::createApp(BSTR app_id,
                                           BSTR brand_code,
                                           BSTR language,
                                           BSTR ap) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::createInstalledApp(BSTR app_id) {
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::createAllInstalledApps() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_displayLanguage(BSTR* language) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::put_displayLanguage(BSTR language) {
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::put_parentHWND(ULONG_PTR hwnd) {
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::get_length(int* number) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_appWeb(int index, IDispatch** app_web) {
  DCHECK(index == 0);
  DCHECK(app_web);

  Microsoft::WRL::ComPtr<IAppWeb> app(this);
  *app_web = app.Detach();
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::initialize() {
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::checkForUpdate() {
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::download() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::install() {
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::pause() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::resume() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::cancel() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::downloadPackage(BSTR app_id,
                                                 BSTR package_name) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_currentState(VARIANT* current_state) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_appId(BSTR* app_id) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_currentVersionWeb(IDispatch** current) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_nextVersionWeb(IDispatch** next) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_command(BSTR command_id,
                                             IDispatch** command) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_currentState(IDispatch** current_state) {
  DCHECK(current_state);

  Microsoft::WRL::ComPtr<ICurrentState> state(this);
  *current_state = state.Detach();
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::launch() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::uninstall() {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_serverInstallDataIndex(BSTR* language) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::put_serverInstallDataIndex(BSTR language) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::get_stateValue(LONG* state_value) {
  DCHECK(state_value);

  *state_value = STATE_NO_UPDATE;
  return S_OK;
}
STDMETHODIMP LegacyOnDemandImpl::get_availableVersion(BSTR* available_version) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_bytesDownloaded(ULONG* bytes_downloaded) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_totalBytesToDownload(
    ULONG* total_bytes_to_download) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_downloadTimeRemainingMs(
    LONG* download_time_remaining_ms) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_nextRetryTime(ULONGLONG* next_retry_time) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_installProgress(
    LONG* install_progress_percentage) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_installTimeRemainingMs(
    LONG* install_time_remaining_ms) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_isCanceled(VARIANT_BOOL* is_canceled) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_errorCode(LONG* error_code) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_extraCode1(LONG* extra_code1) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_completionMessage(
    BSTR* completion_message) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_installerResultCode(
    LONG* installer_result_code) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_installerResultExtraCode1(
    LONG* installer_result_extra_code1) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_postInstallLaunchCommandLine(
    BSTR* post_install_launch_command_line) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_postInstallUrl(BSTR* post_install_url) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::get_postInstallAction(
    LONG* post_install_action) {
  return E_NOTIMPL;
}

STDMETHODIMP LegacyOnDemandImpl::GetTypeInfoCount(UINT*) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::GetTypeInfo(UINT, LCID, ITypeInfo**) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::GetIDsOfNames(REFIID,
                                               LPOLESTR*,
                                               UINT,
                                               LCID,
                                               DISPID*) {
  return E_NOTIMPL;
}
STDMETHODIMP LegacyOnDemandImpl::Invoke(DISPID,
                                        REFIID,
                                        LCID,
                                        WORD,
                                        DISPPARAMS*,
                                        VARIANT*,
                                        EXCEPINFO*,
                                        UINT*) {
  return E_NOTIMPL;
}

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
