// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INSPECTOR_ISSUE_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INSPECTOR_ISSUE_H_

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_node.h"

namespace blink {

struct WebInspectorIssue {
  WebString code;

  WebInspectorIssue() = default;
  WebInspectorIssue(const WebString& code) : code(code) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_INSPECTOR_ISSUE_H_
