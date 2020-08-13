// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/swap_promise_monitor.h"
#include "base/check.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/swap_promise_manager.h"

namespace cc {

SwapPromiseMonitor::SwapPromiseMonitor(SwapPromiseManager* swap_promise_manager)
    : swap_promise_manager_(swap_promise_manager), host_impl_(nullptr) {
  DCHECK(swap_promise_manager);
  swap_promise_manager_->InsertSwapPromiseMonitor(this);
}

SwapPromiseMonitor::SwapPromiseMonitor(LayerTreeHostImpl* host_impl)
    : swap_promise_manager_(nullptr), host_impl_(host_impl) {
  DCHECK(host_impl);
  host_impl_->InsertSwapPromiseMonitor(this);
}

SwapPromiseMonitor::~SwapPromiseMonitor() {
  if (swap_promise_manager_)
    swap_promise_manager_->RemoveSwapPromiseMonitor(this);
  if (host_impl_)
    host_impl_->RemoveSwapPromiseMonitor(this);
}

}  // namespace cc
