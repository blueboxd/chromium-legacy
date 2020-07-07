// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_

#include "base/sequence_checker.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class SystemClipboard;
class ClipboardPromise;

// Interface for reading async-clipboard-compatible types from the sanitized
// System Clipboard as a Blob.
//
// Reading a type from the system clipboard to a Blob is accomplished by:
// (1) Reading the item from the system clipboard.
// (2) Encoding the blob's contents.
// (3) Writing the contents to a blob.
class ClipboardReader : public GarbageCollected<ClipboardReader> {
 public:
  // Returns nullptr if there is no implementation for the given mime_type.
  static ClipboardReader* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type,
                                 ClipboardPromise* promise);
  virtual ~ClipboardReader();

  // Reads from the system clipboard and encodes on a background thread.
  virtual void Read() = 0;

  void Trace(Visitor* visitor) const;

 protected:
  // TaskRunner for interacting with the system clipboard.
  const scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner_;

  explicit ClipboardReader(SystemClipboard* system_clipboard,
                           ClipboardPromise* promise);

  SystemClipboard* system_clipboard() { return system_clipboard_; }
  Member<ClipboardPromise> promise_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Access to the global sanitized system clipboard.
  Member<SystemClipboard> system_clipboard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_READER_H_
