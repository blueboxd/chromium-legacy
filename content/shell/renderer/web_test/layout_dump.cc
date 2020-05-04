// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/layout_dump.h"

#include "base/check.h"
#include "base/strings/stringprintf.h"
#include "content/shell/renderer/web_test/web_frame_test_proxy.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_frame_content_dumper.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace content {

using blink::WebFrame;
using blink::WebFrameContentDumper;
using blink::WebLocalFrame;
using blink::WebSize;

namespace {

std::string DumpFrameHeaderIfNeeded(WebLocalFrame* frame) {
  std::string result;

  // Add header for all but the main frame. Skip empty frames.
  if (frame->Parent() && !frame->GetDocument().DocumentElement().IsNull()) {
    result.append("\n--------\nFrame: '");
    auto* frame_proxy = static_cast<WebFrameTestProxy*>(frame->Client());
    result.append(frame_proxy->GetFrameNameForWebTests());
    result.append("'\n--------\n");
  }

  return result;
}

std::string DumpFrameScrollPosition(WebLocalFrame* frame) {
  std::string result;
  WebSize offset = frame->GetScrollOffset();
  if (offset.width > 0 || offset.height > 0) {
    if (frame->Parent()) {
      auto* frame_proxy = static_cast<WebFrameTestProxy*>(frame->Client());
      result = std::string("frame '") + frame_proxy->GetFrameNameForWebTests() +
               "' ";
    }
    base::StringAppendF(&result, "scrolled to %d,%d\n", offset.width,
                        offset.height);
  }

  return result;
}

}  // namespace

std::string DumpLayoutAsString(WebLocalFrame* frame,
                               const WebTestRuntimeFlags& flags) {
  DCHECK(frame);
  std::string result;

  if (flags.dump_as_text()) {
    result = DumpFrameHeaderIfNeeded(frame);
    result += frame->GetDocument().ContentAsTextForTesting().Utf8();
    result += "\n";
  } else if (flags.dump_as_markup()) {
    DCHECK(!flags.is_printing());
    result = DumpFrameHeaderIfNeeded(frame);
    result += WebFrameContentDumper::DumpAsMarkup(frame).Utf8();
    result += "\n";
  } else if (flags.dump_as_layout()) {
    if (frame->Parent() == nullptr) {
      WebFrameContentDumper::LayoutAsTextControls layout_text_behavior =
          WebFrameContentDumper::kLayoutAsTextNormal;
      if (flags.is_printing())
        layout_text_behavior |= WebFrameContentDumper::kLayoutAsTextPrinting;
      result = WebFrameContentDumper::DumpLayoutTreeAsText(frame,
                                                           layout_text_behavior)
                   .Utf8();
    }
    result += DumpFrameScrollPosition(frame);
  }

  return result;
}

}  // namespace content
