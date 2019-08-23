// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"
#include "weblayer/public/web_main.h"
#include "weblayer/public/web_profile.h"
#include "weblayer/shell/browser/web_shell.h"

#if defined(OS_WIN)
#include "base/base_paths_win.h"
#elif defined(OS_LINUX)
#include "base/nix/xdg_util.h"
#endif

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::CommandLine::StringVector& args = command_line->GetArgs();

#if defined(OS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#endif

  if (args.empty())
    return GURL("https://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
}

class ShellWebMainDelegate : public weblayer::WebMainDelegate {
 public:
  void PreMainMessageLoopRun() override {
    InitializeProfiles();

    weblayer::WebShell::Initialize();

    weblayer::WebShell::CreateNewWindow(profile_.get(), GetStartupURL(),
                                        gfx::Size());
  }

  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {
    weblayer::WebShell::SetMainMessageLoopQuitClosure(std::move(quit_closure));
  }

 private:
  void InitializeProfiles() {
    base::FilePath path;
#if defined(OS_WIN)
    CHECK(base::PathService::Get(base::DIR_LOCAL_APP_DATA, &path));
    path = path.AppendASCII("web_shell");
#elif defined(OS_LINUX)
    std::unique_ptr<base::Environment> env(base::Environment::Create());
    base::FilePath config_dir(base::nix::GetXDGDirectory(
        env.get(), base::nix::kXdgConfigHomeEnvVar, base::nix::kDotConfigDir));
    path = config_dir.AppendASCII("web_shell");
#elif defined(OS_ANDROID)
    CHECK(base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path));
    path = path.AppendASCII("web_shell");
#else
    NOTIMPLEMENTED();
#endif

    if (!base::PathExists(path))
      base::CreateDirectory(path);

    profile_ = weblayer::WebProfile::Create(path);

    // TODO: create an incognito profile as well.
  }

  std::unique_ptr<weblayer::WebProfile> profile_;
};

weblayer::WebMainParams CreateMainParams() {
  static const base::NoDestructor<ShellWebMainDelegate> weblayer_delegate;
  weblayer::WebMainParams params;
  params.delegate = const_cast<ShellWebMainDelegate*>(&(*weblayer_delegate));

  base::PathService::Get(base::DIR_EXE, &params.log_filename);
  params.log_filename = params.log_filename.AppendASCII("weblayer_shell.log");

  params.pak_name = "weblayer_shell.pak";

  params.brand = "weblayer_shell";
  params.full_version = WEBLAYER_SHELL_VERSION;
  params.major_version = WEBLAYER_SHELL_MAJOR_VERSION;
  return params;
}

}  // namespace

#if defined(OS_WIN)

#if defined(WIN_CONSOLE_APP)
int main() {
  return WebMain(CreateMainParams());
#else
int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  return WebMain(CreateMainParams(), instance);
#endif
}

#else

int main(int argc, const char** argv) {
  return WebMain(CreateMainParams(), argc, argv);
}

#endif  // OS_POSIX
