// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_shim/app_shim_handler_mac.h"

namespace apps {
namespace {
AppShimHandler* g_app_shim_handler = nullptr;
}  // namespace

// static
AppShimHandler* AppShimHandler::Get() {
  return g_app_shim_handler;
}

// static
void AppShimHandler::Set(AppShimHandler* handler) {
  g_app_shim_handler = handler;
}

}  // namespace apps
