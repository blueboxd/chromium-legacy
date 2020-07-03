// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_

#include "base/util/type_safety/id_type.h"

namespace performance_manager {

// A strongly typed wrapper for the id returned by RenderProcessHost::GetID().
using RenderProcessHostId = util::IdType32<class RenderProcessHostIdTag>;

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RENDER_PROCESS_HOST_ID_H_
