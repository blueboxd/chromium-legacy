// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
#define COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_

#include <memory>

#include "base/callback.h"
#include "components/arc/session/arc_client_adapter.h"
#include "components/arc/session/file_system_status.h"

namespace arc {

// Enum that describes which native bridge mode is used to run arm binaries on
// x86.
enum class ArcBinaryTranslationType {
  NONE,
  HOUDINI,
  NDK_TRANSLATION,
};

// Returns an adapter for arcvm.
std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapter();

using FileSystemStatusRewriter =
    base::RepeatingCallback<void(FileSystemStatus*)>;
std::unique_ptr<ArcClientAdapter> CreateArcVmClientAdapterForTesting(
    const FileSystemStatusRewriter& rewriter);

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_ARC_VM_CLIENT_ADAPTER_H_
