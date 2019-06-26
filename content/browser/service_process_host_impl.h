// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_SERVICE_PROCESS_HOST_IMPL_H_

#include "base/macros.h"
#include "base/threading/sequence_bound.h"
#include "content/common/service_control.mojom.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

// Implementation of the public ServiceProcessHost API. These objects should
// only be created on the UI thread via calls to ServiceProcessHost::Create().
class ServiceProcessHostImpl : public ServiceProcessHost {
 public:
  ServiceProcessHostImpl(base::StringPiece service_interface_name,
                         mojo::ScopedMessagePipeHandle receiving_pipe,
                         Options options);
  ~ServiceProcessHostImpl() override;

 private:
  class IOThreadState;

  mojo::Remote<mojom::ServiceControl> remote_control_;
  base::SequenceBound<IOThreadState> io_thread_state_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_PROCESS_HOST_IMPL_H_
