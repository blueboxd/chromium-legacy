// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/setup/setup.h"

int main(int argc, const char* argv[]) {
  return updater::setup::UpdaterSetupMain(argc, argv);
}
