// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/clipboard/clipboard_reader.h"

#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/image-encoders/image_encoder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {  // anonymous namespace for ClipboardReader's derived classes.

// Reads an image from the System Clipboard as a blob with image/png content.
class ClipboardImageReader final : public ClipboardReader {
 public:
  ClipboardImageReader(SystemClipboard* system_clipboard)
      : ClipboardReader(system_clipboard) {}
  ~ClipboardImageReader() override = default;

  Blob* ReadFromSystem() override {
    SkBitmap bitmap =
        system_clipboard()->ReadImage(mojom::ClipboardBuffer::kStandard);

    // Encode bitmap to Vector<uint8_t> on the main thread.
    SkPixmap pixmap;
    bitmap.peekPixels(&pixmap);

    // Set encoding options to favor speed over size.
    SkPngEncoder::Options options;
    options.fZLibLevel = 1;
    options.fFilterFlags = SkPngEncoder::FilterFlag::kNone;

    Vector<uint8_t> png_data;
    if (!ImageEncoder::Encode(&png_data, pixmap, options))
      return nullptr;

    return Blob::Create(png_data.data(), png_data.size(), kMimeTypeImagePng);
  }
};

// Reads an image from the System Clipboard as a blob with text/plain content.
class ClipboardTextReader final : public ClipboardReader {
 public:
  ClipboardTextReader(SystemClipboard* system_clipboard)
      : ClipboardReader(system_clipboard) {}
  ~ClipboardTextReader() override = default;

  Blob* ReadFromSystem() override {
    String plain_text =
        system_clipboard()->ReadPlainText(mojom::ClipboardBuffer::kStandard);

    // Encode WTF String to UTF-8, the standard text format for blobs.
    StringUTF8Adaptor utf_text(plain_text);
    return Blob::Create(reinterpret_cast<const uint8_t*>(utf_text.data()),
                        utf_text.size(), kMimeTypeTextPlain);
  }
};

}  // anonymous namespace

// ClipboardReader functions.

// static
ClipboardReader* ClipboardReader::Create(SystemClipboard* system_clipboard,
                                         const String& mime_type) {
  if (mime_type == kMimeTypeImagePng)
    return MakeGarbageCollected<ClipboardImageReader>(system_clipboard);
  if (mime_type == kMimeTypeTextPlain)
    return MakeGarbageCollected<ClipboardTextReader>(system_clipboard);

  // The MIME type is not supported.
  return nullptr;
}

ClipboardReader::ClipboardReader(SystemClipboard* system_clipboard)
    : system_clipboard_(system_clipboard) {}

ClipboardReader::~ClipboardReader() = default;

void ClipboardReader::Trace(Visitor* visitor) {
  visitor->Trace(system_clipboard_);
}

}  // namespace blink
