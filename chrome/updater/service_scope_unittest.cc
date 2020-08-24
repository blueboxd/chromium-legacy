// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/service_scope.h"

#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "chrome/updater/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace updater {

TEST(ServiceScope, GetProcessScope) {
  base::test::ScopedCommandLine original_command_line;
  {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->RemoveSwitch(kSystemSwitch);
    DCHECK_EQ(GetProcessScope(), ServiceScope::kUser);
    command_line->AppendSwitch(kSystemSwitch);
    DCHECK_EQ(GetProcessScope(), ServiceScope::kSystem);
  }
}

}  // namespace updater
