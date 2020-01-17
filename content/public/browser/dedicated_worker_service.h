// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_SERVICE_H_

#include "base/observer_list_types.h"
#include "base/util/type_safety/id_type.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

using DedicatedWorkerId = util::IdType64<class DedicatedWorkerTag>;

// An interface that allows to subscribe to the lifetime of dedicated workers.
// The service is owned by the StoragePartition and lives on the UI thread.
class CONTENT_EXPORT DedicatedWorkerService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when a dedicated worker has started/stopped.
    virtual void OnWorkerStarted(
        DedicatedWorkerId dedicated_worker_id,
        int worker_process_id,
        GlobalFrameRoutingId ancestor_render_frame_host_id) = 0;
    virtual void OnBeforeWorkerTerminated(
        DedicatedWorkerId dedicated_worker_id,
        GlobalFrameRoutingId ancestor_render_frame_host_id) = 0;
  };

  // Adds/removes an observer.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  virtual ~DedicatedWorkerService() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEDICATED_WORKER_SERVICE_H_
