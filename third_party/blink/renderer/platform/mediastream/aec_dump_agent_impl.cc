// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/mediastream/aec_dump_agent_impl.h"

#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

// static
std::unique_ptr<AecDumpAgentImpl> AecDumpAgentImpl::Create(Delegate* delegate) {
  // TODO(crbug.com/704136): Use GetInterfaceProvider() here.
  if (!Platform::Current()->GetConnector())  // Can be true in unit tests.
    return nullptr;

  mojo::Remote<mojom::blink::AecDumpManager> manager;
  Platform::Current()->GetConnector()->Connect(
      Platform::Current()->GetBrowserServiceName(),
      manager.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<AecDumpAgent> remote;
  auto receiver = remote.InitWithNewPipeAndPassReceiver();

  manager->Add(std::move(remote));

  return base::WrapUnique(new AecDumpAgentImpl(delegate, std::move(receiver)));
}

AecDumpAgentImpl::AecDumpAgentImpl(
    Delegate* delegate,
    mojo::PendingReceiver<mojom::blink::AecDumpAgent> receiver)
    : delegate_(delegate), receiver_(this, std::move(receiver)) {}

AecDumpAgentImpl::~AecDumpAgentImpl() = default;

void AecDumpAgentImpl::Start(base::File dump_file) {
  delegate_->OnStartDump(std::move(dump_file));
}

void AecDumpAgentImpl::Stop() {
  delegate_->OnStopDump();
}

}  // namespace blink
