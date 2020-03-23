// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/app/shell_main_params.h"

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
#include "weblayer/public/main.h"
#include "weblayer/public/profile.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/shell/common/shell_switches.h"

namespace weblayer {

namespace {

GURL GetStartupURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kNoInitialNavigation))
    return GURL();

#if defined(OS_ANDROID)
  // Delay renderer creation on Android until surface is ready.
  return GURL();
#else
  const base::CommandLine::StringVector& args = command_line->GetArgs();

  if (args.empty())
    return GURL("https://www.google.com/");

  GURL url(args[0]);
  if (url.is_valid() && url.has_scheme())
    return url;

  return net::FilePathToFileURL(
      base::MakeAbsoluteFilePath(base::FilePath(args[0])));
#endif
}

class MainDelegateImpl : public MainDelegate {
 public:
  void PreMainMessageLoopRun() override {
    InitializeProfile();

    Shell::Initialize();

    Shell::CreateNewWindow(profile_.get(), GetStartupURL(), gfx::Size());
  }

  void PostMainMessageLoopRun() override { DestroyProfile(); }

  void SetMainMessageLoopQuitClosure(base::OnceClosure quit_closure) override {
    Shell::SetMainMessageLoopQuitClosure(std::move(quit_closure));
  }

 private:
  void InitializeProfile() {
    profile_ = Profile::Create("web_shell");

    // TODO: create an incognito profile as well.
  }

  void DestroyProfile() { profile_.reset(); }

  std::unique_ptr<Profile> profile_;
};

}  // namespace

MainParams CreateMainParams() {
  static const base::NoDestructor<MainDelegateImpl> weblayer_delegate;
  MainParams params;
  params.delegate = const_cast<MainDelegateImpl*>(&(*weblayer_delegate));

  base::PathService::Get(base::DIR_EXE, &params.log_filename);
  params.log_filename = params.log_filename.AppendASCII("weblayer_shell.log");

  params.pak_name = "weblayer.pak";

  return params;
}

}  //  namespace weblayer
