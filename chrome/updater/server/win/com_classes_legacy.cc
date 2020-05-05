// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This macro is used in <wrl/module.h>. Since only the COM functionality is
// used here (while WinRT is not being used), define this macro to optimize
// compilation of <wrl/module.h> for COM-only.
#ifndef __WRL_CLASSIC_COM_STRICT__
#define __WRL_CLASSIC_COM_STRICT__
#endif  // __WRL_CLASSIC_COM_STRICT__

#include "chrome/updater/server/win/com_classes_legacy.h"

#include <windows.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/updater/server/win/server.h"

namespace updater {

LegacyOnDemandImpl::LegacyOnDemandImpl()
    : task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::WithBaseSyncPrimitives()})) {}

LegacyOnDemandImpl::~LegacyOnDemandImpl() = default;

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
  set_app_id(base::UTF16ToASCII(app_id));
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
  DCHECK_EQ(index, 0);
  DCHECK(app_web);

  Microsoft::WRL::ComPtr<IAppWeb> app(this);
  *app_web = app.Detach();
  return S_OK;
}

STDMETHODIMP LegacyOnDemandImpl::initialize() {
  return S_OK;
}

// Invokes the in-process update service on the main sequence. Forwards the
// callbacks to a sequenced task runner. |obj| is bound to this object.
STDMETHODIMP LegacyOnDemandImpl::checkForUpdate() {
  using LegacyOnDemandImplPtr = Microsoft::WRL::ComPtr<LegacyOnDemandImpl>;
  auto com_server = ComServerApp::Instance();
  com_server->main_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<UpdateService> update_service,
             LegacyOnDemandImplPtr obj) {
            update_service->Update(
                obj->app_id(), UpdateService::Priority::kForeground,
                base::BindRepeating(
                    [](LegacyOnDemandImplPtr obj,
                       UpdateService::UpdateState state_update) {
                      obj->task_runner_->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &LegacyOnDemandImpl::UpdateStateCallback, obj,
                              state_update));
                    },
                    obj),
                base::BindOnce(
                    [](LegacyOnDemandImplPtr obj,
                       UpdateService::Result result) {
                      obj->task_runner_->PostTask(
                          FROM_HERE,
                          base::BindOnce(
                              &LegacyOnDemandImpl::UpdateResultCallback, obj,
                              result));
                    },
                    obj));
          },
          com_server->service(), LegacyOnDemandImplPtr(this)));
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

  *state_value = GetOnDemandCurrentState();
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
  *error_code = GetOnDemandError();

  return S_OK;
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

void LegacyOnDemandImpl::UpdateStateCallback(
    UpdateService::UpdateState state_update) {
  base::AutoLock lock{lock_};
  state_update_ = state_update;
}

void LegacyOnDemandImpl::UpdateResultCallback(UpdateService::Result result) {
  base::AutoLock lock{lock_};
  result_ = result;
}

// Returns the state of update as seen by the on-demand client:
// - if the repeading callback has been received: returns the specific state.
// - if the completion callback has been received, but no repeating callback,
//   then it returns STATE_ERROR. This is an error state and it indicates that
//   update is not going to be further handled and repeating callbacks posted.
// - if no callback has been received at all: returns STATE_INIT.
CurrentState LegacyOnDemandImpl::GetOnDemandCurrentState() const {
  base::AutoLock lock{lock_};
  if (state_update_) {
    switch (state_update_.value().state) {
      case UpdateService::UpdateState::State::kUnknown:
        // Fall through.
      case UpdateService::UpdateState::State::kNotStarted:
        return STATE_INIT;
      case UpdateService::UpdateState::State::kCheckingForUpdates:
        return STATE_CHECKING_FOR_UPDATE;
      case UpdateService::UpdateState::State::kUpdateAvailable:
        return STATE_UPDATE_AVAILABLE;
      case UpdateService::UpdateState::State::kDownloading:
        return STATE_DOWNLOADING;
      case UpdateService::UpdateState::State::kInstalling:
        return STATE_INSTALLING;
      case UpdateService::UpdateState::State::kUpdated:
        return STATE_INSTALL_COMPLETE;
      case UpdateService::UpdateState::State::kNoUpdate:
        return STATE_NO_UPDATE;
      case UpdateService::UpdateState::State::kUpdateError:
        return STATE_ERROR;
    }
  } else if (result_) {
    DCHECK_NE(result_.value(), UpdateService::Result::kSuccess);
    return STATE_ERROR;
  } else {
    return STATE_INIT;
  }
}

// TODO(crbug.com/1073659): improve the error handling.
HRESULT LegacyOnDemandImpl::GetOnDemandError() const {
  base::AutoLock lock{lock_};
  if (!result_)
    return S_OK;

  return (result_.value() == UpdateService::Result::kSuccess) ? S_OK : E_FAIL;
}

}  // namespace updater
