// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/raw_system_clipboard.h"

#include "mojo/public/cpp/base/big_buffer.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom-blink.h"
namespace blink {

RawSystemClipboard::RawSystemClipboard(LocalFrame* frame)
    : clipboard_(frame->DomWindow()) {
  frame->GetBrowserInterfaceBroker().GetInterface(
      clipboard_.BindNewPipeAndPassReceiver(
          frame->GetTaskRunner(TaskType::kUserInteraction)));
}

void RawSystemClipboard::ReadAvailableFormatNames(
    mojom::blink::RawClipboardHost::ReadAvailableFormatNamesCallback callback) {
  clipboard_->ReadAvailableFormatNames(std::move(callback));
}

void RawSystemClipboard::Read(
    const String& type,
    mojom::blink::RawClipboardHost::ReadCallback callback) {
  clipboard_->Read(type, std::move(callback));
}

void RawSystemClipboard::Write(const String& type, mojo_base::BigBuffer data) {
  clipboard_->Write(type, std::move(data));
}

void RawSystemClipboard::CommitWrite() {
  clipboard_->CommitWrite();
}

void RawSystemClipboard::Trace(Visitor* visitor) {
  visitor->Trace(clipboard_);
}

}  // namespace blink
