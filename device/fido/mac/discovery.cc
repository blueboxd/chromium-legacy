// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/mac/discovery.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/mac/authenticator.h"

namespace device {
namespace fido {
namespace mac {

FidoTouchIdDiscovery::FidoTouchIdDiscovery(
    AuthenticatorConfig authenticator_config)
    : FidoDiscoveryBase(FidoTransportProtocol::kInternal),
      authenticator_config_(std::move(authenticator_config)),
      weak_factory_(this) {}

FidoTouchIdDiscovery::~FidoTouchIdDiscovery() = default;

void FidoTouchIdDiscovery::Start() {
  DCHECK(!authenticator_);
  if (!observer()) {
    return;
  }

  if (!TouchIdAuthenticator::IsAvailable(authenticator_config_)) {
    observer()->DiscoveryStarted(this, /*success=*/false);
    return;
  }

  observer()->DiscoveryStarted(this, /*success=*/true);

  // Start() is currently invoked synchronously in the
  // FidoRequestHandler ctor. Invoke AddAuthenticator() asynchronously
  // to avoid hairpinning FidoRequestHandler::AuthenticatorAdded()
  // before the request handler has an observer.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoTouchIdDiscovery::AddAuthenticator,
                                weak_factory_.GetWeakPtr()));
}

void FidoTouchIdDiscovery::AddAuthenticator() {
  DCHECK(TouchIdAuthenticator::IsAvailable(authenticator_config_));
  authenticator_ = TouchIdAuthenticator::Create(authenticator_config_);
  observer()->AuthenticatorAdded(this, authenticator_.get());
}

}  // namespace mac
}  // namespace fido
}  // namespace device
