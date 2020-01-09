// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/keymaster/cert_store_bridge.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/platform/platform_channel.h"

namespace arc {
namespace keymaster {

CertStoreBridge::CertStoreBridge(content::BrowserContext* context)
    : context_(context), weak_ptr_factory_(this) {
  VLOG(2) << "CertStoreBridge::CertStoreBridge";
}

CertStoreBridge::~CertStoreBridge() {
  VLOG(2) << "CertStoreBridge::~CertStoreBridge";
  security_token_operation_.reset();
}

void CertStoreBridge::GetSecurityTokenOperation(
    mojom::SecurityTokenOperationRequest operation_request,
    GetSecurityTokenOperationCallback callback) {
  VLOG(2) << "CertStoreBridge::GetSecurityTokenOperation";
  security_token_operation_ = std::make_unique<SecurityTokenOperationBridge>(
      context_, mojo::PendingReceiver<mojom::SecurityTokenOperation>(
                    std::move(operation_request)));
  std::move(callback).Run();
}

void CertStoreBridge::BindToInvitation(mojo::OutgoingInvitation* invitation) {
  VLOG(2) << "CertStoreBridge::BootstrapMojoConnection";

  mojo::ScopedMessagePipeHandle pipe =
      invitation->AttachMessagePipe("arc-cert-store-pipe");

  cert_store_proxy_.Bind(
      mojo::InterfacePtrInfo<keymaster::mojom::CertStoreInstance>(
          std::move(pipe), 0u));
  VLOG(2) << "Bound remote CertStoreInstance interface to pipe.";
  cert_store_proxy_.set_connection_error_handler(base::BindOnce(
      &mojo::InterfacePtr<keymaster::mojom::CertStoreInstance>::reset,
      base::Unretained(&cert_store_proxy_)));
}

void CertStoreBridge::OnBootstrapMojoConnection(bool result) {
  if (!result) {
    cert_store_proxy_.reset();
    return;
  }

  auto binding =
      std::make_unique<mojo::Binding<keymaster::mojom::CertStoreHost>>(this);
  mojo::InterfacePtr<keymaster::mojom::CertStoreHost> host_proxy;
  binding->Bind(mojo::MakeRequest(&host_proxy));

  cert_store_proxy_->Init(
      std::move(host_proxy),
      base::BindOnce(&CertStoreBridge::OnConnectionReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(binding)));
}

void CertStoreBridge::OnConnectionReady(
    std::unique_ptr<mojo::Binding<mojom::CertStoreHost>> binding) {
  VLOG(2) << "CertStoreBridge::OnConnectionReady";
  DCHECK(!binding_);
  binding->set_connection_error_handler(base::BindOnce(
      &CertStoreBridge::OnConnectionClosed, base::Unretained(this)));
  binding_ = std::move(binding);
}

void CertStoreBridge::OnConnectionClosed() {
  VLOG(2) << "CertStoreBridge::OnConnectionClosed";
  DCHECK(binding_);
  binding_.reset();
}

}  // namespace keymaster
}  // namespace arc
