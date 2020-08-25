// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/bloom/public/cpp/bloom_controller.h"
#include "base/check_op.h"

namespace chromeos {
namespace bloom {

namespace {

BloomController* g_instance = nullptr;

}  // namespace

// static
BloomController* BloomController::Get() {
  return g_instance;
}

BloomController::BloomController() {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

BloomController::~BloomController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

}  // namespace bloom
}  // namespace chromeos
