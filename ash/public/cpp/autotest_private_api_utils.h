// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_PRIVATE_API_UTILS_H_
#define ASH_PUBLIC_CPP_AUTOTEST_PRIVATE_API_UTILS_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/frame/header_view.h"

namespace aura {
class Window;
}

// Utility functions for autotest private APIs and ShellTestAPI.
namespace ash {

// Get application windows, windows that are shown in overview grid.
ASH_EXPORT std::vector<aura::Window*> GetAppWindowList();

// Get HeaderView from application windows.
ASH_EXPORT ash::HeaderView* GetHeaderViewForWindow(aura::Window* window);
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_PRIVATE_API_UTILS_H_
