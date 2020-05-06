// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/ime/public/cpp/shared_lib/interfaces.h"

extern "C" {

// This is the exposed function from shared library, used by IME service to
// acquire the instance of ImeEngineMainEntry.
__attribute__((visibility("default"))) chromeos::ime::ImeEngineMainEntry*
CreateImeMainEntry(chromeos::ime::ImeCrosPlatform*) {
  return nullptr;
}
}
