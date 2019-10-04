// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/performance_manager_lock_observer.h"

#include "chrome/browser/performance_manager/graph/frame_node_impl.h"
#include "chrome/browser/performance_manager/performance_manager_impl.h"
#include "content/public/browser/lock_observer.h"

namespace performance_manager {

namespace {

void SetHoldsWebLock(int render_process_id,
                     int render_frame_id,
                     bool holds_web_lock,
                     GraphImpl* graph) {
  FrameNodeImpl* frame_node =
      graph->GetFrameNodeById(render_process_id, render_frame_id);
  if (frame_node)
    frame_node->SetHoldsWebLock(holds_web_lock);
}

}  // namespace

PerformanceManagerLockObserver::PerformanceManagerLockObserver() = default;
PerformanceManagerLockObserver::~PerformanceManagerLockObserver() = default;

void PerformanceManagerLockObserver::OnFrameStartsHoldingWebLocks(
    int render_process_id,
    int render_frame_id) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&SetHoldsWebLock, render_process_id,
                                render_frame_id, true));
}

void PerformanceManagerLockObserver::OnFrameStopsHoldingWebLocks(
    int render_process_id,
    int render_frame_id) {
  PerformanceManagerImpl::CallOnGraphImpl(
      FROM_HERE, base::BindOnce(&SetHoldsWebLock, render_process_id,
                                render_frame_id, false));
}

}  // namespace performance_manager
