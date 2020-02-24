// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/policy/policy_features.h"

#include "base/command_line.h"
#include "ios/chrome/browser/chrome_switches.h"

bool IsEnterprisePolicyEnabled() {
  // This feature is controlled via the command line because policy must be
  // initialized before about:flags or field trials. Using a command line flag
  // is the only way to control this feature at runtime.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(switches::kEnableEnterprisePolicy);
}
