// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_service_impl.h"

namespace content {

DedicatedWorkerServiceImpl::DedicatedWorkerServiceImpl() = default;

DedicatedWorkerServiceImpl::~DedicatedWorkerServiceImpl() = default;

void DedicatedWorkerServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DedicatedWorkerServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

DedicatedWorkerId DedicatedWorkerServiceImpl::GenerateNextDedicatedWorkerId() {
  return dedicated_worker_id_generator_.GenerateNextId();
}

void DedicatedWorkerServiceImpl::NotifyWorkerStarted(
    DedicatedWorkerId dedicated_worker_id,
    int worker_process_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id) {
  for (Observer& observer : observers_) {
    observer.OnWorkerStarted(dedicated_worker_id, worker_process_id,
                             ancestor_render_frame_host_id);
  }
}

void DedicatedWorkerServiceImpl::NotifyWorkerTerminating(
    DedicatedWorkerId dedicated_worker_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id) {
  for (Observer& observer : observers_) {
    observer.OnBeforeWorkerTerminated(dedicated_worker_id,
                                      ancestor_render_frame_host_id);
  }
}

}  // namespace content
