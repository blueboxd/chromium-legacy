// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_shutdown_observer.h"

#include "third_party/blink/renderer/core/dom/document.h"

namespace blink {

DocumentShutdownObserver::DocumentShutdownObserver(Document* document) {
  SetDocument(document);
}

void DocumentShutdownObserver::ObserverListWillBeCleared() {
  document_ = nullptr;
}

void DocumentShutdownObserver::SetDocument(Document* document) {
  if (document == document_)
    return;

  if (document_)
    document_->DocumentShutdownObserverList().RemoveObserver(this);

  document_ = document;

  if (document_)
    document_->DocumentShutdownObserverList().AddObserver(this);
}

void DocumentShutdownObserver::Trace(Visitor* visitor) {
  visitor->Trace(document_);
}

}  // namespace blink
