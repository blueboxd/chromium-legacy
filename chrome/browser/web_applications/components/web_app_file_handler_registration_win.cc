// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"

#include <algorithm>
#include <iterator>
#include <set>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "net/base/filename_util.h"

namespace {

// Returns the app-specific-launcher filename to be used for |app_name|.
base::FilePath GetAppSpecificLauncherFilename(const base::string16& app_name) {
  // Remove any characters that are illegal in Windows filenames.
  base::FilePath sanitized_app_name =
      web_app::internals::GetSanitizedFileName(app_name);

  // If |sanitized_app_name| is a reserved filename, append '_' to allow its use
  // as the launcher filename (e.g. "nul" => "nul_"). If |sanitized_app_name|
  // starts with a reserved filename followed by '.', replace all '.' characters
  // with '_' (e.g. "nul.l" => "nul_l"). This is needed because anything after
  // '.' is interpreted as part of a file extension for the purpose of
  // identifying reserved filenames, so appending '_' fails to legitimize a
  // reserved filename when a '.' is present (e.g. "nul.l_" is rejected).
  if (net::IsReservedNameOnWindows(sanitized_app_name.value())) {
    base::string16 allowed_filename = sanitized_app_name.value();
    if (!base::ReplaceChars(allowed_filename, L".", L"_", &allowed_filename))
      allowed_filename.append(L"_");
    sanitized_app_name = base::FilePath(allowed_filename);
  }

  // On Windows 8+, add .exe extension. On Windows 7, where the launcher
  // filename is used as the app's display name in the Open With menu, omit the
  // extension.
  if (base::win::GetVersion() > base::win::Version::WIN7)
    return sanitized_app_name.AddExtension(L"exe");
  return sanitized_app_name;
}

}  // namespace

namespace web_app {

const base::FilePath::StringPieceType kLastBrowserFile(
    FILE_PATH_LITERAL("Last Browser"));

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

// See https://docs.microsoft.com/en-us/windows/win32/com/-progid--key for
// the allowed characters in a prog_id. Since the prog_id is stored in the
// Windows registry, the mapping between a given profile+app_id and a prog_id
// can not be changed.
base::string16 GetProgIdForApp(Profile* profile, const AppId& app_id) {
  base::string16 prog_id = install_static::GetBaseAppId();
  std::string app_specific_part(
      base::UTF16ToUTF8(profile->GetPath().BaseName().value()));
  app_specific_part.append(app_id);
  uint32_t hash = base::PersistentHash(app_specific_part);
  prog_id.push_back('.');
  prog_id.append(base::ASCIIToUTF16(base::NumberToString(hash)));
  return prog_id;
}

void RegisterFileHandlersWithOsTask(
    const AppId& app_id,
    const std::string& app_name,
    const base::FilePath& profile_path,
    const base::string16& app_prog_id,
    const std::set<std::string>& file_extensions) {
  base::FilePath web_app_path =
      GetWebAppDataDirectory(profile_path, app_id, GURL());
  base::string16 utf16_app_name = base::UTF8ToUTF16(app_name);
  base::FilePath icon_path =
      internals::GetIconFilePath(web_app_path, utf16_app_name);
  base::FilePath pwa_launcher_path = GetChromePwaLauncherPath();
  base::FilePath app_specific_launcher_path =
      web_app_path.Append(GetAppSpecificLauncherFilename(utf16_app_name));

  // Create a hard link to the chrome pwa launcher app. Delete any pre-existing
  // version of the file first.
  base::DeleteFile(app_specific_launcher_path, /*recursive=*/false);
  if (!base::CreateWinHardLink(app_specific_launcher_path, pwa_launcher_path) &&
      !base::CopyFile(pwa_launcher_path, app_specific_launcher_path)) {
    DPLOG(ERROR) << "Unable to copy the generic PWA launcher";
    return;
  }
  base::CommandLine app_specific_launcher_command(app_specific_launcher_path);
  app_specific_launcher_command.AppendArg("%1");
  app_specific_launcher_command.AppendSwitchPath(switches::kProfileDirectory,
                                                 profile_path.BaseName());
  app_specific_launcher_command.AppendSwitchASCII(switches::kAppId, app_id);
  std::set<base::string16> file_exts;
  // Copy |file_extensions| to a string16 set in O(n) time by hinting that
  // the appended elements should go at the end of the set.
  std::transform(file_extensions.begin(), file_extensions.end(),
                 std::inserter(file_exts, file_exts.end()),
                 [](const std::string& ext) { return base::UTF8ToUTF16(ext); });

  ShellUtil::AddFileAssociations(
      app_prog_id, app_specific_launcher_command, utf16_app_name,
      utf16_app_name + STRING16_LITERAL(" File"), icon_path, file_exts);
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const std::set<std::string>& file_extensions,
                                const std::set<std::string>& mime_types) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterFileHandlersWithOsTask, app_id, app_name,
                     profile->GetPath(), GetProgIdForApp(profile, app_id),
                     file_extensions));
}

void UnregisterFileHandlersWithOs(const AppId& app_id, Profile* profile) {
  // Need to delete the app-specific-launcher file, since uninstall may not
  // remove the web application directory. This must be done before cleaning up
  // the registry, since the app-specific-launcher path is retrieved from the
  // registry.

  base::string16 prog_id = GetProgIdForApp(profile, app_id);
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(prog_id);

  ShellUtil::DeleteFileAssociations(prog_id);

  // Need to delete the hardlink file as well, since extension uninstall
  // by default doesn't remove the web application directory.
  if (!app_specific_launcher_path.empty()) {
    base::PostTask(
        FROM_HERE,
        {base::ThreadPool(), base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(IgnoreResult(&base::DeleteFile),
                       app_specific_launcher_path, /*recursively=*/false));
  }
}

void UpdateChromeExePath(const base::FilePath& user_data_dir) {
  DCHECK(!user_data_dir.empty());
  base::FilePath chrome_exe_path;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe_path))
    return;
  const base::FilePath::StringType& chrome_exe_path_str =
      chrome_exe_path.value();
  DCHECK(!chrome_exe_path_str.empty());
  base::WriteFile(
      user_data_dir.Append(kLastBrowserFile),
      reinterpret_cast<const char*>(chrome_exe_path_str.data()),
      chrome_exe_path_str.size() * sizeof(base::FilePath::CharType));
}

}  // namespace web_app
