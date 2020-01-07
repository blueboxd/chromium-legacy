// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_SERVICE_IMPL_H_
#define COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_SERVICE_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/flat_set.h"
#include "base/unguessable_token.h"
#include "components/paint_preview/public/paint_preview_compositor_service.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace paint_preview {

class PaintPreviewCompositorServiceImpl : public PaintPreviewCompositorService {
 public:
  explicit PaintPreviewCompositorServiceImpl(
      mojo::Remote<mojom::PaintPreviewCompositorCollection> remote,
      base::OnceClosure disconnect_closure);
  ~PaintPreviewCompositorServiceImpl() override;

  // PaintPreviewCompositorService Implementation.
  std::unique_ptr<PaintPreviewCompositorClient> CreateCompositor(
      base::OnceClosure connected_closure) override;
  bool HasActiveClients() const override;

  // Marks the compositor associated with |token| as deleted in the
  // |active_clients_| set.
  void MarkCompositorAsDeleted(const base::UnguessableToken& token);

  bool IsServiceBoundAndConnected() const;

  // Test method to validate internal state.
  const base::flat_set<base::UnguessableToken>& ActiveClientsForTesting() const;

  PaintPreviewCompositorServiceImpl(const PaintPreviewCompositorServiceImpl&) =
      delete;
  PaintPreviewCompositorServiceImpl& operator=(
      const PaintPreviewCompositorServiceImpl&) = delete;

 private:
  void OnCompositorCreated(const base::UnguessableToken& token);

  void DisconnectHandler();

  mojo::Remote<mojom::PaintPreviewCompositorCollection> compositor_service_;
  base::flat_set<base::UnguessableToken> active_clients_;
  base::OnceClosure user_disconnect_closure_;

  base::WeakPtrFactory<PaintPreviewCompositorServiceImpl> weak_ptr_factory_{
      this};
};

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_BROWSER_PAINT_PREVIEW_COMPOSITOR_SERVICE_IMPL_H_
