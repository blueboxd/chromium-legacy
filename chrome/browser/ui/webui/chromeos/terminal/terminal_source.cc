// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/terminal/terminal_source.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/mime_util.h"

namespace {
// TODO(crbug.com/846546): Initially set to load crosh, but change to
// terminal when it is available.
constexpr base::FilePath::CharType kTerminalRoot[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets/crosh_builtin");
constexpr base::FilePath::CharType kDefaultFile[] =
    FILE_PATH_LITERAL("html/crosh.html");
constexpr char kDefaultMime[] = "text/html";

void ReadFile(const base::FilePath& path,
              const content::URLDataSource::GotDataCallback& callback) {
  std::string content;
  CHECK(base::ReadFileToString(path, &content)) << path;
  scoped_refptr<base::RefCountedString> response =
      base::RefCountedString::TakeString(&content);
  callback.Run(response.get());
}
}  // namespace

std::string TerminalSource::GetSource() {
  return chrome::kChromeUITerminalHost;
}

void TerminalSource::StartDataRequest(
    const std::string& path,
    const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  base::FilePath file_path(kTerminalRoot);
  if (path.empty()) {
    file_path = file_path.Append(kDefaultFile);
  } else {
    file_path = file_path.Append(path);
  }
  base::PostTaskWithTraits(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&ReadFile, file_path, callback));
}

std::string TerminalSource::GetMimeType(const std::string& path) {
  std::string mime_type(kDefaultMime);
  std::string ext = base::FilePath(path).Extension();
  if (!ext.empty())
    net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type);
  return mime_type;
}
