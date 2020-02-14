// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/raw_system_clipboard.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

RawSystemClipboard::RawSystemClipboard(LocalFrame* frame) {
  frame->GetBrowserInterfaceBroker().GetInterface(
      clipboard_.BindNewPipeAndPassReceiver());
}

void RawSystemClipboard::Write(const String& type, mojo_base::BigBuffer data) {
  clipboard_->Write(type, std::move(data));
}

void RawSystemClipboard::CommitWrite() {
  clipboard_->CommitWrite();
}

}  // namespace blink
