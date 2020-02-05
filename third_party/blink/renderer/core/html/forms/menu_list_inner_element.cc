// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/menu_list_inner_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

MenuListInnerElement::MenuListInnerElement(Document& document)
    : HTMLDivElement(document) {
  SetHasCustomStyleCallbacks();
}

scoped_refptr<ComputedStyle>
MenuListInnerElement::CustomStyleForLayoutObject() {
  const ComputedStyle& parent_style = OwnerShadowHost()->ComputedStyleRef();
  scoped_refptr<ComputedStyle> style =
      ComputedStyle::CreateAnonymousStyleWithDisplay(parent_style,
                                                     EDisplay::kNone);
  return style;
}

}  // namespace blink
