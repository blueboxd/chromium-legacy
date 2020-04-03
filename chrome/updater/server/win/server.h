// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVER_WIN_SERVER_H_
#define CHROME_UPDATER_SERVER_WIN_SERVER_H_

#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "chrome/updater/server/win/updater_idl.h"

namespace updater {

// TODO(crbug.com/1065712): these Impl classes don't have to be
// visible in the updater namespace. Additionally, there is some code
// duplication for the registration and unregistration code in both server and
// service_main compilation units.
//
// This class implements the legacy Omaha3 interfaces as expected by Chrome's
// on-demand client.
class LegacyOnDemandImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IGoogleUpdate3Web,
          IAppBundleWeb,
          IAppWeb,
          ICurrentState,
          IDispatch> {
 public:
  LegacyOnDemandImpl() = default;
  LegacyOnDemandImpl(const LegacyOnDemandImpl&) = delete;
  LegacyOnDemandImpl& operator=(const LegacyOnDemandImpl&) = delete;

  // Overrides for IGoogleUpdate3Web.
  IFACEMETHODIMP createAppBundleWeb(IDispatch** app_bundle_web) override;

  // Overrides for IAppBundleWeb.
  IFACEMETHODIMP createApp(BSTR app_id,
                           BSTR brand_code,
                           BSTR language,
                           BSTR ap) override;
  IFACEMETHODIMP createInstalledApp(BSTR app_id) override;
  IFACEMETHODIMP createAllInstalledApps() override;
  IFACEMETHODIMP get_displayLanguage(BSTR* language) override;
  IFACEMETHODIMP put_displayLanguage(BSTR language) override;
  IFACEMETHODIMP put_parentHWND(ULONG_PTR hwnd) override;
  IFACEMETHODIMP get_length(int* number) override;
  IFACEMETHODIMP get_appWeb(int index, IDispatch** app_web) override;
  IFACEMETHODIMP initialize() override;
  IFACEMETHODIMP checkForUpdate() override;
  IFACEMETHODIMP download() override;
  IFACEMETHODIMP install() override;
  IFACEMETHODIMP pause() override;
  IFACEMETHODIMP resume() override;
  IFACEMETHODIMP cancel() override;
  IFACEMETHODIMP downloadPackage(BSTR app_id, BSTR package_name) override;
  IFACEMETHODIMP get_currentState(VARIANT* current_state) override;

  // Overrides for IAppWeb.
  IFACEMETHODIMP get_appId(BSTR* app_id) override;
  IFACEMETHODIMP get_currentVersionWeb(IDispatch** current) override;
  IFACEMETHODIMP get_nextVersionWeb(IDispatch** next) override;
  IFACEMETHODIMP get_command(BSTR command_id, IDispatch** command) override;
  IFACEMETHODIMP get_currentState(IDispatch** current_state) override;
  IFACEMETHODIMP launch() override;
  IFACEMETHODIMP uninstall() override;
  IFACEMETHODIMP get_serverInstallDataIndex(BSTR* language) override;
  IFACEMETHODIMP put_serverInstallDataIndex(BSTR language) override;

  // Overrides for ICurrentState.
  IFACEMETHODIMP get_stateValue(LONG* state_value) override;
  IFACEMETHODIMP get_availableVersion(BSTR* available_version) override;
  IFACEMETHODIMP get_bytesDownloaded(ULONG* bytes_downloaded) override;
  IFACEMETHODIMP get_totalBytesToDownload(
      ULONG* total_bytes_to_download) override;
  IFACEMETHODIMP get_downloadTimeRemainingMs(
      LONG* download_time_remaining_ms) override;
  IFACEMETHODIMP get_nextRetryTime(ULONGLONG* next_retry_time) override;
  IFACEMETHODIMP get_installProgress(
      LONG* install_progress_percentage) override;
  IFACEMETHODIMP get_installTimeRemainingMs(
      LONG* install_time_remaining_ms) override;
  IFACEMETHODIMP get_isCanceled(VARIANT_BOOL* is_canceled) override;
  IFACEMETHODIMP get_errorCode(LONG* error_code) override;
  IFACEMETHODIMP get_extraCode1(LONG* extra_code1) override;
  IFACEMETHODIMP get_completionMessage(BSTR* completion_message) override;
  IFACEMETHODIMP get_installerResultCode(LONG* installer_result_code) override;
  IFACEMETHODIMP get_installerResultExtraCode1(
      LONG* installer_result_extra_code1) override;
  IFACEMETHODIMP get_postInstallLaunchCommandLine(
      BSTR* post_install_launch_command_line) override;
  IFACEMETHODIMP get_postInstallUrl(BSTR* post_install_url) override;
  IFACEMETHODIMP get_postInstallAction(LONG* post_install_action) override;

  // Overrides for IDispatch.
  IFACEMETHODIMP GetTypeInfoCount(UINT*) override;
  IFACEMETHODIMP GetTypeInfo(UINT, LCID, ITypeInfo**) override;
  IFACEMETHODIMP GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override;
  IFACEMETHODIMP Invoke(DISPID,
                        REFIID,
                        LCID,
                        WORD,
                        DISPPARAMS*,
                        VARIANT*,
                        EXCEPINFO*,
                        UINT*) override;

 private:
  ~LegacyOnDemandImpl() override = default;
};

// This class implements the ICompleteStatus interface and exposes it as a COM
// object.
class CompleteStatusImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ICompleteStatus> {
 public:
  CompleteStatusImpl(int code, const base::string16& message)
      : code_(code), message_(message) {}
  CompleteStatusImpl(const CompleteStatusImpl&) = delete;
  CompleteStatusImpl& operator=(const CompleteStatusImpl&) = delete;

  // Overrides for ICompleteStatus.
  IFACEMETHODIMP get_statusCode(LONG* code) override;
  IFACEMETHODIMP get_statusMessage(BSTR* message) override;

 private:
  const int code_;
  const base::string16 message_;

  ~CompleteStatusImpl() override = default;
};

// This class implements the IUpdater interface and exposes it as a COM object.
class UpdaterImpl
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IUpdater> {
 public:
  UpdaterImpl() = default;
  UpdaterImpl(const UpdaterImpl&) = delete;
  UpdaterImpl& operator=(const UpdaterImpl&) = delete;

  // Overrides for IUpdater.
  IFACEMETHODIMP CheckForUpdate(const base::char16* app_id) override;
  IFACEMETHODIMP Register(const base::char16* app_id,
                          const base::char16* brand_code,
                          const base::char16* tag,
                          const base::char16* version,
                          const base::char16* existence_checker_path) override;
  IFACEMETHODIMP Update(const base::char16* app_id) override;
  IFACEMETHODIMP UpdateAll(IUpdaterObserver* observer) override;

 private:
  ~UpdaterImpl() override = default;
};

class App;

scoped_refptr<App> AppServerInstance();

}  // namespace updater

#endif  // CHROME_UPDATER_SERVER_WIN_SERVER_H_
