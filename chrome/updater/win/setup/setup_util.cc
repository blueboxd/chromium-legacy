// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/setup_util.h"

#include <string>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "chrome/updater/server/win/updater_idl.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/task_scheduler.h"

namespace updater {

namespace {

constexpr base::char16 kTaskName[] = L"UpdateApps";
constexpr base::char16 kTaskDescription[] = L"Update all applications.";

}  // namespace

bool RegisterUpdateAppsTask(const base::CommandLine& run_command) {
  auto task_scheduler = TaskScheduler::CreateInstance();
  if (!task_scheduler->RegisterTask(
          kTaskName, kTaskDescription, run_command,
          TaskScheduler::TriggerType::TRIGGER_TYPE_HOURLY, true)) {
    LOG(ERROR) << "RegisterUpdateAppsTask failed.";
    return false;
  }
  VLOG(1) << "RegisterUpdateAppsTask succeeded.";
  return true;
}

void UnregisterUpdateAppsTask() {
  auto task_scheduler = TaskScheduler::CreateInstance();
  task_scheduler->DeleteTask(kTaskName);
}

base::string16 GetComServerClsidRegistryPath(REFCLSID clsid) {
  return base::StrCat(
      {L"Software\\Classes\\CLSID\\", base::win::String16FromGUID(clsid)});
}

base::string16 GetComServiceClsid() {
  return base::win::String16FromGUID(__uuidof(UpdaterServiceClass));
}

base::string16 GetComServiceClsidRegistryPath() {
  return base::StrCat({L"Software\\Classes\\CLSID\\", GetComServiceClsid()});
}

base::string16 GetComServiceAppidRegistryPath() {
  return base::StrCat({L"Software\\Classes\\AppID\\", GetComServiceClsid()});
}

base::string16 GetComIidRegistryPath(REFIID iid) {
  return base::StrCat(
      {L"Software\\Classes\\Interface\\", base::win::String16FromGUID(iid)});
}

base::string16 GetComTypeLibRegistryPath(REFIID iid) {
  return base::StrCat(
      {L"Software\\Classes\\TypeLib\\", base::win::String16FromGUID(iid)});
}

std::vector<base::FilePath> ParseFilesFromDeps(const base::FilePath& deps) {
  constexpr size_t kDepsFileSizeMax = 0x2000;  // 8KB.
  std::string contents;
  if (!base::ReadFileToStringWithMaxSize(deps, &contents, kDepsFileSizeMax))
    return {};
  const base::flat_set<const base::char16*,
                       CaseInsensitiveASCIICompare<base::string16>>
      exclude_extensions = {L".pdb", L".js"};
  std::vector<base::FilePath> result;
  for (const auto& line :
       base::SplitString(contents, "\r\n", base::TRIM_WHITESPACE,
                         base::SPLIT_WANT_NONEMPTY)) {
    const auto filename =
        base::FilePath(base::ASCIIToUTF16(line)).NormalizePathSeparators();
    if (!base::Contains(exclude_extensions,
                        filename.FinalExtension().c_str())) {
      result.push_back(filename);
    }
  }
  return result;
}

}  // namespace updater
