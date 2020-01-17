// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_

#include "base/sequence_checker.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file_reader_loader_client.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

class ClipboardPromise;
class FileReaderLoader;
class SystemClipboard;

// Interface for writing async-clipboard-compatible types as a Blob to the
// System Clipboard, asynchronously.
//
// Writing a Blob's data to the system clipboard is accomplished by:
// (1) Reading the blob's contents using a FileReaderLoader.
// (2) Decoding the blob's contents to avoid RCE in native applications that may
//     take advantage of vulnerabilities in their decoders.
// (3) Writing the blob's decoded contents to the system clipboard.
class ClipboardWriter : public GarbageCollected<ClipboardWriter>,
                        public FileReaderLoaderClient {
 public:
  static ClipboardWriter* Create(SystemClipboard* system_clipboard,
                                 const String& mime_type,
                                 bool allow_without_sanitization,
                                 ClipboardPromise* promise);
  ~ClipboardWriter() override;

  static bool IsValidType(const String& type);
  void WriteToSystem(Blob*);

  // FileReaderLoaderClient.
  void DidStartLoading() override;
  void DidReceiveData() override;
  void DidFinishLoading() override;
  void DidFail(FileErrorCode) override;

  void Trace(Visitor*);

 protected:
  ClipboardWriter(SystemClipboard* system_clipboard, ClipboardPromise* promise);

  virtual void DecodeOnBackgroundThread(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      DOMArrayBuffer* raw_data) = 0;
  // This ClipboardPromise owns this.
  Member<ClipboardPromise> promise_;
  // Ensure that System Clipboard operations occur on the main thread.
  SEQUENCE_CHECKER(sequence_checker_);

  SystemClipboard* system_clipboard() { return system_clipboard_; }

 private:
  // TaskRunner for interacting with the system clipboard.
  const scoped_refptr<base::SingleThreadTaskRunner> clipboard_task_runner_;
  // TaskRunner for reading files.
  const scoped_refptr<base::SingleThreadTaskRunner> file_reading_task_runner_;
  // This FileReaderLoader will load the Blob.
  std::unique_ptr<FileReaderLoader> file_reader_;
  // Access to the global system clipboard.
  Member<SystemClipboard> system_clipboard_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CLIPBOARD_CLIPBOARD_WRITER_H_
