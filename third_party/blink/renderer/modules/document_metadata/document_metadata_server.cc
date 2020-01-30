// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/document_metadata/document_metadata_server.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/document_metadata/document_metadata_extractor.h"

namespace blink {

DocumentMetadataServer::DocumentMetadataServer(LocalFrame& frame)
    : frame_(frame) {}

void DocumentMetadataServer::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<mojom::blink::DocumentMetadata> receiver) {
  DCHECK(frame);

  // TODO(wychen): remove BindMojoReceiver pattern, and make this a service
  // associated with frame lifetime.
  mojo::MakeSelfOwnedReceiver(std::make_unique<DocumentMetadataServer>(*frame),
                              std::move(receiver));
}

void DocumentMetadataServer::GetEntities(GetEntitiesCallback callback) {
  if (!frame_ || !frame_->GetDocument()) {
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(
      DocumentMetadataExtractor::Extract(*frame_->GetDocument()));
}

}  // namespace blink
