// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/zygote/common/zygote_handle.h"

#include "services/service_manager/zygote/host/zygote_communication_linux.h"

namespace service_manager {
namespace {

// Intentionally leaked.
ZygoteHandle g_generic_zygote = nullptr;
ZygoteHandle g_unsandboxed_zygote = nullptr;

}  // namespace

ZygoteHandle CreateGenericZygote(ZygoteLaunchCallback launch_cb) {
  CHECK(!g_generic_zygote);
  g_generic_zygote =
      new ZygoteCommunication(ZygoteCommunication::ZygoteType::kSandboxed);
  g_generic_zygote->Init(std::move(launch_cb));
  return g_generic_zygote;
}

ZygoteHandle GetGenericZygote() {
  CHECK(g_generic_zygote);
  return g_generic_zygote;
}

ZygoteHandle CreateUnsandboxedZygote(ZygoteLaunchCallback launch_cb) {
  CHECK(!g_unsandboxed_zygote);
  g_unsandboxed_zygote =
      new ZygoteCommunication(ZygoteCommunication::ZygoteType::kUnsandboxed);
  g_unsandboxed_zygote->Init(std::move(launch_cb));
  return g_unsandboxed_zygote;
}

ZygoteHandle GetUnsandboxedZygote() {
  CHECK(g_unsandboxed_zygote);
  return g_unsandboxed_zygote;
}

}  // namespace service_manager
