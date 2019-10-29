// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/scenic_gpu_service.h"

#include "base/bind.h"
#include "mojo/public/c/system/message_pipe.h"

namespace ui {

namespace {

// Fulfills a PendingReceiver<T> using a PendingRemote<T>.
//
// Messages queued on the PendingReceiver's message pipe are preserved and will
// be eventually delivered to the remote end of PendingRemote<T>'s.
//
// PendingRemote<T> must be a brand new interface; i.e., it not have been
// previously used to send a message.
template <typename Interface>
void FulfillPendingReceiver(mojo::PendingReceiver<Interface> receiver,
                            mojo::PendingRemote<Interface> remote) {
  MojoResult result =
      mojo::FuseMessagePipes(remote.PassPipe(), receiver.PassPipe());
  DCHECK_EQ(result, MOJO_RESULT_OK);
}

}  // namespace

ScenicGpuService::ScenicGpuService(
    mojo::PendingReceiver<mojom::ScenicGpuHost> gpu_host_receiver)
    : gpu_host_receiver_(std::move(gpu_host_receiver)) {}

ScenicGpuService::~ScenicGpuService() {}

base::RepeatingCallback<void(mojo::PendingReceiver<mojom::ScenicGpuService>)>
ScenicGpuService::GetBinderCallback() {
  return base::BindRepeating(&ScenicGpuService::AddReceiver,
                             weak_ptr_factory_.GetWeakPtr());
}

void ScenicGpuService::Initialize(
    mojo::PendingRemote<mojom::ScenicGpuHost> gpu_host) {
  FulfillPendingReceiver(std::move(gpu_host_receiver_), std::move(gpu_host));
}

void ScenicGpuService::AddReceiver(
    mojo::PendingReceiver<mojom::ScenicGpuService> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

}  // namespace ui
