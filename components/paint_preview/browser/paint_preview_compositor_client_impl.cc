// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/paint_preview_compositor_client_impl.h"

#include <utility>

#include "base/callback.h"

namespace paint_preview {

PaintPreviewCompositorClientImpl::PaintPreviewCompositorClientImpl(
    base::WeakPtr<PaintPreviewCompositorServiceImpl> service)
    : service_(service) {}

PaintPreviewCompositorClientImpl::~PaintPreviewCompositorClientImpl() {
  NotifyServiceOfInvalidation();
}

const base::Optional<base::UnguessableToken>&
PaintPreviewCompositorClientImpl::Token() const {
  return token_;
}

void PaintPreviewCompositorClientImpl::SetDisconnectHandler(
    base::OnceClosure closure) {
  user_disconnect_closure_ = std::move(closure);
}

void PaintPreviewCompositorClientImpl::BeginComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    mojom::PaintPreviewCompositor::BeginCompositeCallback callback) {
  compositor_->BeginComposite(std::move(request), std::move(callback));
}

void PaintPreviewCompositorClientImpl::BitmapForFrame(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    mojom::PaintPreviewCompositor::BitmapForFrameCallback callback) {
  compositor_->BitmapForFrame(frame_guid, clip_rect, scale_factor,
                              std::move(callback));
}

void PaintPreviewCompositorClientImpl::SetRootFrameUrl(const GURL& url) {
  compositor_->SetRootFrameUrl(url);
}

bool PaintPreviewCompositorClientImpl::IsBoundAndConnected() const {
  return compositor_.is_bound() && compositor_.is_connected();
}

mojo::PendingReceiver<mojom::PaintPreviewCompositor>
PaintPreviewCompositorClientImpl::BindNewPipeAndPassReceiver() {
  return compositor_.BindNewPipeAndPassReceiver();
}

PaintPreviewCompositorClientImpl::OnCompositorCreatedCallback
PaintPreviewCompositorClientImpl::BuildCompositorCreatedCallback(
    base::OnceClosure user_closure,
    OnCompositorCreatedCallback service_callback) {
  return base::BindOnce(&PaintPreviewCompositorClientImpl::OnCompositorCreated,
                        weak_ptr_factory_.GetWeakPtr(), std::move(user_closure),
                        std::move(service_callback));
}

void PaintPreviewCompositorClientImpl::OnCompositorCreated(
    base::OnceClosure user_closure,
    OnCompositorCreatedCallback service_callback,
    const base::UnguessableToken& token) {
  token_ = token;
  std::move(user_closure).Run();
  std::move(service_callback).Run(token);
  compositor_.set_disconnect_handler(
      base::BindOnce(&PaintPreviewCompositorClientImpl::DisconnectHandler,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PaintPreviewCompositorClientImpl::NotifyServiceOfInvalidation() {
  if (service_ && token_.has_value())
    service_->MarkCompositorAsDeleted(token_.value());
}

void PaintPreviewCompositorClientImpl::DisconnectHandler() {
  if (user_disconnect_closure_)
    std::move(user_disconnect_closure_).Run();
  NotifyServiceOfInvalidation();
  compositor_.reset();
}

}  // namespace paint_preview
