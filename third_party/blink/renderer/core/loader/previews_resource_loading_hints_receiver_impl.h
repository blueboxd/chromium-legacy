// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_RECEIVER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_RECEIVER_IMPL_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/loader/previews_resource_loading_hints.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

// Created and attached to a LocalFrame when the frame receives a
// PreviewsResourceLoadingHintsReceiver interface receiver from the browser
// process.
class PreviewsResourceLoadingHintsReceiverImpl
    : public mojom::blink::PreviewsResourceLoadingHintsReceiver {
 public:
  PreviewsResourceLoadingHintsReceiverImpl(
      mojo::PendingReceiver<mojom::blink::PreviewsResourceLoadingHintsReceiver>
          receiver,
      Document* document);
  ~PreviewsResourceLoadingHintsReceiverImpl() override;

 private:
  void SetResourceLoadingHints(mojom::blink::PreviewsResourceLoadingHintsPtr
                                   resource_loading_hints) override;

  // TODO(tbansal): https://crbug.com/800641. Consider using a RevocableBinding.
  mojo::Receiver<mojom::blink::PreviewsResourceLoadingHintsReceiver> receiver_;

  WeakPersistent<Document> document_;

  DISALLOW_COPY_AND_ASSIGN(PreviewsResourceLoadingHintsReceiverImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LOADER_PREVIEWS_RESOURCE_LOADING_HINTS_RECEIVER_IMPL_H_
