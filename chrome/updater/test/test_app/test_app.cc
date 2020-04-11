// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/test/test_app/test_app.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/updater/test/test_app/constants.h"
#include "chrome/updater/util.h"

namespace updater {

namespace {

int ParseCommandLine(const base::CommandLine* command_line) {
  if (command_line->HasSwitch(kInstallUpdaterSwitch))
    InstallUpdater();

  if (command_line->HasSwitch(kRegisterToUpdaterSwitch))
    RegisterToUpdater();

  if (command_line->HasSwitch(kForegroundUpdateSwitch))
    DoForegroundUpdate();

  return 0;
}

}  // namespace

int TestAppMain(int argc, const char** argv) {
  base::CommandLine::Init(argc, argv);
  updater::InitLogging(FILE_PATH_LITERAL("test_app.log"));

  return ParseCommandLine(base::CommandLine::ForCurrentProcess());
}

}  // namespace updater
