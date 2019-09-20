// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/de_jelly.h"

#include "base/command_line.h"
#include "components/viz/common/switches.h"

namespace viz {

bool DeJellyEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDeJelly);
}

bool DeJellyActive() {
  // TODO(ericrk): Android specific bits to be added in a follow-up CL.
  return DeJellyEnabled();
}

float DeJellyScreenWidth() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDeJellyScreenWidth);
  if (!value.empty())
    return std::atoi(value.c_str());

  // TODO(ericrk): We can automatically handle this on Android. For now return
  // a reasonable default.
  return 1440.0f;
}

float MaxDeJellyHeight() {
  // Not currently configurable.
  return 30.0f;
}

}  // namespace viz
