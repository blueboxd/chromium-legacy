// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/request_queue.h"

#include "base/check_op.h"

namespace x11 {

// static
RequestQueue* RequestQueue::instance_ = nullptr;

RequestQueue::RequestQueue() {
  DCHECK(!instance_);
  instance_ = this;
}

RequestQueue::~RequestQueue() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

// static
RequestQueue* RequestQueue::GetInstance() {
  return instance_;
}

}  // namespace x11
