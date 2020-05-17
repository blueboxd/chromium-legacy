// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/navigator_contacts.h"

#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// static
const char NavigatorContacts::kSupplementName[] = "NavigatorContacts";

// static
NavigatorContacts& NavigatorContacts::From(Navigator& navigator) {
  NavigatorContacts* supplement =
      Supplement<Navigator>::From<NavigatorContacts>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorContacts>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

// static
ContactsManager* NavigatorContacts::contacts(Navigator& navigator) {
  // TODO(finnur): Return null when navigator is detached?
  return NavigatorContacts::From(navigator).contacts();
}

ContactsManager* NavigatorContacts::contacts() {
  if (!contacts_manager_)
    contacts_manager_ = MakeGarbageCollected<ContactsManager>();
  return contacts_manager_;
}

void NavigatorContacts::Trace(Visitor* visitor) const {
  visitor->Trace(contacts_manager_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorContacts::NavigatorContacts(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

}  // namespace blink
