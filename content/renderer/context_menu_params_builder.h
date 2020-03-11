// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CONTEXT_MENU_PARAMS_BUILDER_H_
#define CONTENT_RENDERER_CONTEXT_MENU_PARAMS_BUILDER_H_

namespace blink {
struct WebContextMenuData;
}

namespace content {
struct UntrustworthyContextMenuParams;

class ContextMenuParamsBuilder {
 public:
  static UntrustworthyContextMenuParams Build(
      const blink::WebContextMenuData& data);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CONTEXT_MENU_PARAMS_BUILDER_H_
