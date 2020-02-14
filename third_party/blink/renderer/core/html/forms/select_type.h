// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_TYPE_H_

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

// SelectType class is an abstraction of the MenuList behavior and the ListBox
// behavior of HTMLSelectElement.
class SelectType : public GarbageCollected<SelectType> {
 public:
  // Creates an instance of a SelectType subclass depending on the current mode
  // of |select|.
  static SelectType* Create(HTMLSelectElement& select);
  virtual void Trace(Visitor* visitor);

  virtual void DidSelectOption(HTMLOptionElement* element,
                               HTMLSelectElement::SelectOptionFlags flags,
                               bool should_update_popup);

  // TODO(crbug.com/1052232): Add more virtual functions.

 protected:
  explicit SelectType(HTMLSelectElement& select);

  const Member<HTMLSelectElement> select_;

 private:
  SelectType(const SelectType&) = delete;
  SelectType& operator=(const SelectType&) = delete;
};

}  // namespace blink
#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_FORMS_SELECT_TYPE_H_
