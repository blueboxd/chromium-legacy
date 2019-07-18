// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/stream/aec_dump_agent_impl.h"

#include "content/public/child/child_thread.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

// static
std::unique_ptr<AecDumpAgentImpl> AecDumpAgentImpl::Create(Delegate* delegate) {
  if (!ChildThread::Get())  // Can be true in unit tests.
    return nullptr;

  mojo::Remote<blink::mojom::AecDumpManager> manager;
  ChildThread::Get()->GetConnector()->Connect(
      mojom::kBrowserServiceName, manager.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<AecDumpAgent> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  manager->Add(std::move(remote));

  return base::WrapUnique(new AecDumpAgentImpl(delegate, std::move(receiver)));
}

AecDumpAgentImpl::AecDumpAgentImpl(
    Delegate* delegate,
    mojo::PendingReceiver<blink::mojom::AecDumpAgent> receiver)
    : delegate_(delegate), receiver_(this, std::move(receiver)) {}

AecDumpAgentImpl::~AecDumpAgentImpl() = default;

void AecDumpAgentImpl::Start(base::File dump_file) {
  delegate_->OnStartDump(std::move(dump_file));
}

void AecDumpAgentImpl::Stop() {
  delegate_->OnStopDump();
}

}  // namespace content
