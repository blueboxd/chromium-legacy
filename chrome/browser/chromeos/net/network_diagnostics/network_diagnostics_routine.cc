// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_diagnostics/network_diagnostics_routine.h"

namespace chromeos {
namespace network_diagnostics {

NetworkDiagnosticsRoutine::NetworkDiagnosticsRoutine() = default;

NetworkDiagnosticsRoutine::~NetworkDiagnosticsRoutine() = default;

bool NetworkDiagnosticsRoutine::CanRun() {
  return true;
}

}  // namespace network_diagnostics
}  // namespace chromeos
