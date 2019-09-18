// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/launcher/test_launcher.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "weblayer/test/test_launcher_delegate_impl.h"

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  size_t parallel_jobs = base::NumParallelJobs();
  weblayer::TestLauncherDelegateImpl launcher_delegate;
  return content::LaunchTests(&launcher_delegate, parallel_jobs, argc, argv);
}
