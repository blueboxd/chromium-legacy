// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_

#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class SystemClipboard;

// Interface for reading async-clipboard-compatible types from the System
// Clipboard as a Blob.
//
// Reading a type from the system clipboard to a Blob is accomplished by:
// (1) Reading the item from the system clipboard.
// (2) Encoding the blob's contents.
// (3) Writing the contents to a blob.
class ClipboardReader : public GarbageCollected<ClipboardReader> {
 public:
  static ClipboardReader* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type);
  virtual ~ClipboardReader();

  virtual Blob* ReadFromSystem() = 0;

  void Trace(Visitor* visitor);

 protected:
  explicit ClipboardReader(SystemClipboard* system_clipboard);

  SystemClipboard* system_clipboard() { return system_clipboard_; }

 private:
  // Access to the global system clipboard.
  Member<SystemClipboard> system_clipboard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
