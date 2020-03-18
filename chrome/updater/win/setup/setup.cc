// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup.h"

#include <shlobj.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/win_util.h"
#include "chrome/installer/util/install_service_work_item.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/work_item_list.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/server/win/updater_idl.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/constants.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"
#include "chrome/updater/win/util.h"

namespace updater {

namespace {

const base::char16* kUpdaterFiles[] = {
    L"icudtl.dat",
    L"updater.exe",
    L"uninstall.cmd",
#if defined(COMPONENT_BUILD)
    // TODO(sorin): get the list of component dependencies from a build-time
    // file instead of hardcoding the names of the components here.
    L"base.dll",
    L"base_i18n.dll",
    L"boringssl.dll",
    L"crcrypto.dll",
    L"icui18n.dll",
    L"icuuc.dll",
    L"libc++.dll",
    L"prefs.dll",
    L"protobuf_lite.dll",
    L"url_lib.dll",
    L"zlib.dll",
#endif
};

// Adds work items to register the COM Server with Windows.
void AddComServerWorkItems(HKEY root,
                           const base::FilePath& com_server_path,
                           WorkItemList* list) {
  DCHECK(list);
  if (com_server_path.empty()) {
    LOG(DFATAL) << "com_server_path is invalid.";
    return;
  }

  const base::string16 clsid_reg_path = GetComServerClsidRegistryPath();

  // Delete any old registrations first.
  for (const auto& reg_path : {clsid_reg_path}) {
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
      list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
  }

  list->AddCreateRegKeyWorkItem(root, clsid_reg_path, WorkItem::kWow64Default);
  const base::string16 local_server32_reg_path =
      base::StrCat({clsid_reg_path, L"\\LocalServer32"});
  list->AddCreateRegKeyWorkItem(root, local_server32_reg_path,
                                WorkItem::kWow64Default);

  base::CommandLine run_com_server_command(com_server_path);
  run_com_server_command.AppendSwitch(kServerSwitch);
#if !defined(NDEBUG)
  run_com_server_command.AppendSwitch(kEnableLoggingSwitch);
  run_com_server_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                           "*/chrome/updater/*=2");
#endif

  list->AddSetRegValueWorkItem(
      root, local_server32_reg_path, WorkItem::kWow64Default, L"",
      run_com_server_command.GetCommandLineString(), true);
}

// Adds work items to register the COM Service with Windows.
void AddComServiceWorkItems(const base::FilePath& com_service_path,
                            WorkItemList* list) {
  DCHECK(list);
  DCHECK(::IsUserAnAdmin());
  const HKEY root = HKEY_LOCAL_MACHINE;

  if (com_service_path.empty()) {
    LOG(DFATAL) << "com_service_path is invalid.";
    return;
  }

  const base::string16 clsid_reg_path = GetComServiceClsidRegistryPath();
  const base::string16 appid_reg_path = GetComServiceAppidRegistryPath();

  // Delete any old registrations first.
  for (const auto& reg_path : {clsid_reg_path, appid_reg_path}) {
    for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
      list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
  }

  list->AddWorkItem(new installer::InstallServiceWorkItem(
      kWindowsServiceName, kWindowsServiceName,
      base::CommandLine(com_service_path)));

  list->AddCreateRegKeyWorkItem(root, clsid_reg_path, WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, clsid_reg_path, WorkItem::kWow64Default,
                               L"AppID", GetComServiceClsid(), true);
  list->AddCreateRegKeyWorkItem(root, appid_reg_path, WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(root, appid_reg_path, WorkItem::kWow64Default,
                               L"LocalService", kWindowsServiceName, true);
}

// Adds work items to register the COM Interfaces with Windows.
void AddComInterfacesWorkItems(HKEY root,
                               const base::FilePath& typelib_path,
                               WorkItemList* list) {
  DCHECK(list);
  if (typelib_path.empty()) {
    LOG(DFATAL) << "typelib_path is invalid.";
    return;
  }

  for (const auto iid : {__uuidof(IUpdater), __uuidof(IUpdaterObserver),
                         __uuidof(ICompleteStatus)}) {
    const base::string16 iid_reg_path = GetComIidRegistryPath(iid);
    const base::string16 typelib_reg_path = GetComTypeLibRegistryPath(iid);

    // Delete any old registrations first.
    for (const auto& reg_path : {iid_reg_path, typelib_reg_path}) {
      for (const auto& key_flag : {KEY_WOW64_32KEY, KEY_WOW64_64KEY})
        list->AddDeleteRegKeyWorkItem(root, reg_path, key_flag);
    }

    // Registering the Ole Automation marshaler with the CLSID
    // {00020424-0000-0000-C000-000000000046} as the proxy/stub for the
    // interfaces.
    list->AddCreateRegKeyWorkItem(root, iid_reg_path + L"\\ProxyStubClsid32",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(
        root, iid_reg_path + L"\\ProxyStubClsid32", WorkItem::kWow64Default,
        L"", L"{00020424-0000-0000-C000-000000000046}", true);
    list->AddCreateRegKeyWorkItem(root, iid_reg_path + L"\\TypeLib",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, iid_reg_path + L"\\TypeLib",
                                 WorkItem::kWow64Default, L"",
                                 base::win::String16FromGUID(iid), true);

    // The TypeLib registration for the Ole Automation marshaler.
    list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win32",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win32",
                                 WorkItem::kWow64Default, L"",
                                 typelib_path.value(), true);
    list->AddCreateRegKeyWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win64",
                                  WorkItem::kWow64Default);
    list->AddSetRegValueWorkItem(root, typelib_reg_path + L"\\1.0\\0\\win64",
                                 WorkItem::kWow64Default, L"",
                                 typelib_path.value(), true);
  }
}

}  // namespace

int Setup(bool is_machine) {
  VLOG(1) << __func__ << ", is_machine: " << is_machine;
  DCHECK(!is_machine || ::IsUserAnAdmin());
  HKEY key = is_machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  base::FilePath temp_dir;
  if (!base::GetTempDir(&temp_dir)) {
    LOG(ERROR) << "GetTempDir failed.";
    return -1;
  }
  base::FilePath product_dir;
  if (!GetProductDirectory(&product_dir)) {
    LOG(ERROR) << "GetProductDirectory failed.";
    return -1;
  }
  base::FilePath exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(ERROR) << "PathService failed.";
    return -1;
  }

  installer::SelfCleaningTempDir backup_dir;
  if (!backup_dir.Initialize(temp_dir, L"updater-backup")) {
    LOG(ERROR) << "Failed to initialize the backup dir.";
    return -1;
  }

  const base::FilePath source_dir = exe_path.DirName();

  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  for (const auto* file : kUpdaterFiles) {
    const base::FilePath target_path = product_dir.Append(file);
    const base::FilePath source_path = source_dir.Append(file);
    install_list->AddCopyTreeWorkItem(source_path.value(), target_path.value(),
                                      temp_dir.value(), WorkItem::ALWAYS);
  }

  for (const auto& key_path :
       {GetRegistryKeyClientsUpdater(), GetRegistryKeyClientStateUpdater()}) {
    install_list->AddCreateRegKeyWorkItem(key, key_path,
                                          WorkItem::kWow64Default);
    install_list->AddSetRegValueWorkItem(
        key, key_path, WorkItem::kWow64Default, kRegistryValuePV,
        base::ASCIIToUTF16(UPDATER_VERSION_STRING), true);
    install_list->AddSetRegValueWorkItem(
        key, key_path, WorkItem::kWow64Default, kRegistryValueName,
        base::ASCIIToUTF16(PRODUCT_FULLNAME_STRING), true);
  }

  static constexpr base::FilePath::StringPieceType kUpdaterExe =
      FILE_PATH_LITERAL("updater.exe");
  AddComServerWorkItems(key, product_dir.Append(kUpdaterExe),
                        install_list.get());

  if (is_machine)
    AddComServiceWorkItems(product_dir.Append(kUpdaterExe), install_list.get());

  AddComInterfacesWorkItems(key, product_dir.Append(kUpdaterExe),
                            install_list.get());

  base::CommandLine run_updater_ua_command(product_dir.Append(kUpdaterExe));
  run_updater_ua_command.AppendSwitch(kUpdateAppsSwitch);

  // TODO(sorin) remove "single-process" when the updater COM client works.
  // crbug.com/1053729.
  run_updater_ua_command.AppendSwitch(kSingleProcessSwitch);
#if !defined(NDEBUG)
  run_updater_ua_command.AppendSwitch(kEnableLoggingSwitch);
  run_updater_ua_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                           "*/chrome/updater/*=2");
#endif
  if (!install_list->Do() || !RegisterUpdateAppsTask(run_updater_ua_command)) {
    LOG(ERROR) << "Install failed, rolling back...";
    install_list->Rollback();
    UnregisterUpdateAppsTask();
    LOG(ERROR) << "Rollback complete.";
    return -1;
  }

  VLOG(1) << "Setup succeeded.";
  return 0;
}

}  // namespace updater
