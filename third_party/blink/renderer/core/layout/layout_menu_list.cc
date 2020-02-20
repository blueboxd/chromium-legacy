/*
 * This file is part of the select element layoutObject in WebCore.
 *
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 *           (C) 2009 Torch Mobile Inc. All rights reserved.
 *               (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/layout/layout_menu_list.h"

#include "third_party/blink/renderer/core/html/forms/html_select_element.h"

namespace blink {

LayoutMenuList::LayoutMenuList(Element* element) : LayoutFlexibleBox(element) {
  DCHECK(IsA<HTMLSelectElement>(element));
}

LayoutMenuList::~LayoutMenuList() = default;

bool LayoutMenuList::IsChildAllowed(LayoutObject* object,
                                    const ComputedStyle&) const {
  // For a size=1 <select>, we only render the active option label through the
  // InnerElement. We do not allow adding layout objects for options,
  // optgroups, ::before, or ::after.
  return object->GetNode() == &SelectElement()->InnerElement();
}

HTMLSelectElement* LayoutMenuList::SelectElement() const {
  return To<HTMLSelectElement>(GetNode());
}

PhysicalRect LayoutMenuList::ControlClipRect(
    const PhysicalOffset& additional_offset) const {
  PhysicalRect outer_box = PhysicalContentBoxRect();
  outer_box.offset += additional_offset;
  return outer_box;
}

}  // namespace blink
