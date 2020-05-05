// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/text_input_client_observer.h"

#include <stddef.h>

#include <memory>

#include "content/common/text_input_client_messages.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/render_widget.h"
#include "ipc/ipc_message.h"
#include "third_party/blink/public/web/mac/web_substring_util.h"
#include "third_party/blink/public/web/web_frame_widget.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

TextInputClientObserver::TextInputClientObserver(RenderWidget* render_widget)
    : render_widget_(render_widget) {}

TextInputClientObserver::~TextInputClientObserver() = default;

bool TextInputClientObserver::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TextInputClientObserver, message)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringAtPoint,
                        OnStringAtPoint)
    IPC_MESSAGE_HANDLER(TextInputClientMsg_StringForRange, OnStringForRange)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool TextInputClientObserver::Send(IPC::Message* message) {
  // This class is attached to the main frame RenderWidget, but its messages
  // are not received on RenderWidgetHostImpl, so there's no need to send
  // through RenderWidget or use its routing id.
  return RenderThread::Get()->Send(message);
}

blink::WebFrameWidget* TextInputClientObserver::GetWebFrameWidget() const {
  return static_cast<blink::WebFrameWidget*>(render_widget_->GetWebWidget());
}

blink::WebLocalFrame* TextInputClientObserver::GetFocusedFrame() const {
  if (auto* frame_widget = GetWebFrameWidget()) {
    blink::WebLocalFrame* local_root = frame_widget->LocalRoot();
    blink::WebLocalFrame* focused = local_root->View()->FocusedFrame();
    return focused->LocalRoot() == local_root ? focused : nullptr;
  }
  return nullptr;
}

void TextInputClientObserver::OnStringAtPoint(gfx::Point point) {
  gfx::Point baseline_point;
  NSAttributedString* string = nil;

  if (auto* frame_widget = GetWebFrameWidget()) {
    string = blink::WebSubstringUtil::AttributedWordAtPoint(frame_widget, point,
                                                            baseline_point);
  }

  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringAtPoint(
      MSG_ROUTING_NONE, *encoded.get(), baseline_point));
}

void TextInputClientObserver::OnStringForRange(gfx::Range range) {
  gfx::Point baseline_point;
  NSAttributedString* string = nil;
  blink::WebLocalFrame* frame = GetFocusedFrame();
  // TODO(yabinh): Null check should not be necessary.
  // See crbug.com/304341
  if (frame) {
    string = blink::WebSubstringUtil::AttributedSubstringInRange(
        frame, range.start(), range.length(), &baseline_point);
  }
  std::unique_ptr<const mac::AttributedStringCoder::EncodedString> encoded(
      mac::AttributedStringCoder::Encode(string));
  Send(new TextInputClientReplyMsg_GotStringForRange(
      MSG_ROUTING_NONE, *encoded.get(), baseline_point));
}

}  // namespace content
