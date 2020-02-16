// Copyright 2017 The Chromium Authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_SHUTDOWN_OBSERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_SHUTDOWN_OBSERVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

class Document;

// This class is a base class for classes which observe Document shutdown
// synchronously.
// Note: this functionality is also provided by SynchronousMutationObserver,
// but if you don't need to respond to the other events handled by that class,
// using this class is more efficient.
class CORE_EXPORT DocumentShutdownObserver : public GarbageCollectedMixin {
 public:
  // Called when detaching document.
  virtual void OnDocumentShutdown() = 0;

  // Call before clearing an observer list.
  void ObserverListWillBeCleared();

  Document* GetDocument() const { return document_; }
  void SetDocument(Document*);

  void Trace(Visitor* visitor) override;

 protected:
  DocumentShutdownObserver() = default;
  explicit DocumentShutdownObserver(Document* document);

 private:
  WeakMember<Document> document_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_DOCUMENT_SHUTDOWN_OBSERVER_H_
