// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_RAW_CLIPBOARD_HOST_IMPL_H_
#define CONTENT_BROWSER_FRAME_HOST_RAW_CLIPBOARD_HOST_IMPL_H_

#include "content/browser/frame_host/render_frame_host_impl.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom.h"
#include "ui/base/clipboard/clipboard.h"

namespace ui {
class ScopedClipboardWriter;
}  // namespace ui

namespace content {

class RenderFrameHost;

class CONTENT_EXPORT RawClipboardHostImpl
    : public blink::mojom::RawClipboardHost {
 public:
  static void Create(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver);
  RawClipboardHostImpl(const RawClipboardHostImpl&) = delete;
  RawClipboardHostImpl& operator=(const RawClipboardHostImpl&) = delete;
  ~RawClipboardHostImpl() override;

 private:
  explicit RawClipboardHostImpl(
      mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver);

  // mojom::RawClipboardHost.
  void ReadAvailableFormatNames(
      ReadAvailableFormatNamesCallback callback) override;
  void Write(const base::string16&, mojo_base::BigBuffer) override;
  void CommitWrite() override;

  mojo::Receiver<blink::mojom::RawClipboardHost> receiver_;
  ui::Clipboard* const clipboard_;  // Not owned.
  std::unique_ptr<ui::ScopedClipboardWriter> clipboard_writer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_RAW_CLIPBOARD_HOST_IMPL_H_