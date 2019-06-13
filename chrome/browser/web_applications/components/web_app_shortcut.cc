// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_shortcut.h"

#include <functional>

#include "base/bind.h"
#include "base/i18n/file_util_icu.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

void DeleteShortcutInfoOnUIThread(
    std::unique_ptr<web_app::ShortcutInfo> shortcut_info,
    base::OnceClosure callback) {
  shortcut_info.reset();
  if (callback)
    std::move(callback).Run();
}

}  // namespace

namespace web_app {

ShortcutInfo::ShortcutInfo() = default;

ShortcutInfo::~ShortcutInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ShortcutLocations::ShortcutLocations()
    : on_desktop(false),
      applications_menu_location(APP_MENU_LOCATION_NONE),
      in_quick_launch_bar(false) {}

std::string GenerateApplicationNameFromInfo(const ShortcutInfo& shortcut_info) {
  // TODO(loyso): Remove this empty()/non-empty difference.
  if (shortcut_info.extension_id.empty())
    return GenerateApplicationNameFromURL(shortcut_info.url);

  return GenerateApplicationNameFromAppId(shortcut_info.extension_id);
}

base::FilePath GetWebAppDataDirectory(const base::FilePath& profile_path,
                                      const std::string& extension_id,
                                      const GURL& url) {
  DCHECK(!profile_path.empty());
  base::FilePath app_data_dir(profile_path.Append(chrome::kWebAppDirname));

  if (!extension_id.empty()) {
    return app_data_dir.AppendASCII(
        GenerateApplicationNameFromAppId(extension_id));
  }

  std::string host(url.host());
  std::string scheme(url.has_scheme() ? url.scheme() : "http");
  std::string port(url.has_port() ? url.port() : "80");
  std::string scheme_port(scheme + "_" + port);

#if defined(OS_WIN)
  base::FilePath::StringType host_path(base::UTF8ToUTF16(host));
  base::FilePath::StringType scheme_port_path(base::UTF8ToUTF16(scheme_port));
#elif defined(OS_POSIX)
  base::FilePath::StringType host_path(host);
  base::FilePath::StringType scheme_port_path(scheme_port);
#endif

  return app_data_dir.Append(host_path).Append(scheme_port_path);
}

namespace internals {

void PostShortcutIOTask(base::OnceCallback<void(const ShortcutInfo&)> task,
                        std::unique_ptr<ShortcutInfo> shortcut_info) {
  PostShortcutIOTaskAndReply(std::move(task), std::move(shortcut_info),
                             base::OnceClosure());
}

void PostShortcutIOTaskAndReply(
    base::OnceCallback<void(const ShortcutInfo&)> task,
    std::unique_ptr<ShortcutInfo> shortcut_info,
    base::OnceClosure reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Ownership of |shortcut_info| moves to the Reply, which is guaranteed to
  // outlive the const reference.
  const web_app::ShortcutInfo& shortcut_info_ref = *shortcut_info;
  GetShortcutIOTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(std::move(task), std::cref(shortcut_info_ref)),
      base::BindOnce(&DeleteShortcutInfoOnUIThread, std::move(shortcut_info),
                     std::move(reply)));
}

scoped_refptr<base::TaskRunner> GetShortcutIOTaskRunner() {
  constexpr base::TaskTraits traits = {
      base::MayBlock(), base::TaskPriority::BEST_EFFORT,
      base::TaskShutdownBehavior::BLOCK_SHUTDOWN};

#if defined(OS_WIN)
  return base::CreateCOMSTATaskRunnerWithTraits(
      traits, base::SingleThreadTaskRunnerThreadMode::SHARED);
#else
  return base::CreateTaskRunnerWithTraits(traits);
#endif
}

base::FilePath GetSanitizedFileName(const base::string16& name) {
#if defined(OS_WIN)
  base::string16 file_name = name;
#else
  std::string file_name = base::UTF16ToUTF8(name);
#endif
  base::i18n::ReplaceIllegalCharactersInPath(&file_name, '_');
  return base::FilePath(file_name);
}

base::FilePath GetShortcutDataDir(const web_app::ShortcutInfo& shortcut_info) {
  return web_app::GetWebAppDataDirectory(shortcut_info.profile_path,
                                         shortcut_info.extension_id,
                                         shortcut_info.url);
}

}  // namespace internals

}  // namespace web_app
