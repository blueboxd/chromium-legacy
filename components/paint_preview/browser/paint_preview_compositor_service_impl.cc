// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_compositor_service_impl.h"

#include "components/paint_preview/browser/paint_preview_compositor_client_impl.h"
#include "components/paint_preview/public/paint_preview_compositor_client.h"

namespace paint_preview {

PaintPreviewCompositorServiceImpl::PaintPreviewCompositorServiceImpl(
    mojo::Remote<mojom::PaintPreviewCompositorCollection> remote,
    base::OnceClosure disconnect_handler)
    : compositor_service_(std::move(remote)),
      user_disconnect_closure_(std::move(disconnect_handler)) {
  compositor_service_.set_disconnect_handler(
      base::BindOnce(&PaintPreviewCompositorServiceImpl::DisconnectHandler,
                     weak_ptr_factory_.GetWeakPtr()));
}

// The destructor for the |compositor_service_| will automatically result in any
// active compositors being killed.
PaintPreviewCompositorServiceImpl::~PaintPreviewCompositorServiceImpl() =
    default;

std::unique_ptr<PaintPreviewCompositorClient>
PaintPreviewCompositorServiceImpl::CreateCompositor(
    base::OnceClosure connected_closure) {
  auto compositor = std::make_unique<PaintPreviewCompositorClientImpl>(
      weak_ptr_factory_.GetWeakPtr());
  compositor_service_->CreateCompositor(
      compositor->BindNewPipeAndPassReceiver(),
      compositor->BuildCompositorCreatedCallback(
          std::move(connected_closure),
          base::BindOnce(
              &PaintPreviewCompositorServiceImpl::OnCompositorCreated,
              weak_ptr_factory_.GetWeakPtr())));
  return compositor;
}

bool PaintPreviewCompositorServiceImpl::HasActiveClients() const {
  return !active_clients_.empty();
}

void PaintPreviewCompositorServiceImpl::MarkCompositorAsDeleted(
    const base::UnguessableToken& token) {
  active_clients_.erase(token);
}

bool PaintPreviewCompositorServiceImpl::IsServiceBoundAndConnected() const {
  return compositor_service_.is_bound() && compositor_service_.is_connected();
}

const base::flat_set<base::UnguessableToken>&
PaintPreviewCompositorServiceImpl::ActiveClientsForTesting() const {
  return active_clients_;
}

void PaintPreviewCompositorServiceImpl::OnCompositorCreated(
    const base::UnguessableToken& token) {
  active_clients_.insert(token);
}

void PaintPreviewCompositorServiceImpl::DisconnectHandler() {
  std::move(user_disconnect_closure_).Run();
  compositor_service_.reset();
}

}  // namespace paint_preview
