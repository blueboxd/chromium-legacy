// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/contacts_picker/contacts_manager.h"

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/contacts_picker/contact_info.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace mojo {

template <>
struct TypeConverter<blink::ContactInfo*, blink::mojom::blink::ContactInfoPtr> {
  static blink::ContactInfo* Convert(
      const blink::mojom::blink::ContactInfoPtr& contact);
};

blink::ContactInfo*
TypeConverter<blink::ContactInfo*, blink::mojom::blink::ContactInfoPtr>::
    Convert(const blink::mojom::blink::ContactInfoPtr& contact) {
  blink::ContactInfo* contact_info = blink::ContactInfo::Create();

  if (contact->name.has_value()) {
    Vector<String> names;
    names.ReserveInitialCapacity(contact->name->size());

    for (const String& name : *contact->name)
      names.push_back(name);

    contact_info->setName(names);
  }

  if (contact->email.has_value()) {
    Vector<String> emails;
    emails.ReserveInitialCapacity(contact->email->size());

    for (const String& email : *contact->email)
      emails.push_back(email);

    contact_info->setEmail(emails);
  }

  if (contact->tel.has_value()) {
    Vector<String> numbers;
    numbers.ReserveInitialCapacity(contact->tel->size());

    for (const String& number : *contact->tel)
      numbers.push_back(number);

    contact_info->setTel(numbers);
  }

  return contact_info;
}

}  // namespace mojo

namespace blink {

ContactsManager::ContactsManager() = default;
ContactsManager::~ContactsManager() = default;

mojom::blink::ContactsManagerPtr& ContactsManager::GetContactsManager(
    ScriptState* script_state) {
  if (!contacts_manager_) {
    ExecutionContext::From(script_state)
        ->GetInterfaceProvider()
        ->GetInterface(mojo::MakeRequest(&contacts_manager_));
  }
  return contacts_manager_;
}

ScriptPromise ContactsManager::select(ScriptState* script_state,
                                      const Vector<String>& properties,
                                      ContactsSelectOptions* options) {
  Document* document = To<Document>(ExecutionContext::From(script_state));
  if (!LocalFrame::HasTransientUserActivation(document ? document->GetFrame()
                                                       : nullptr)) {
    return ScriptPromise::Reject(
        script_state, V8ThrowException::CreateTypeError(
                          script_state->GetIsolate(),
                          "A user gesture is required to call this method"));
  }

  if (properties.IsEmpty()) {
    return ScriptPromise::Reject(script_state,
                                 V8ThrowException::CreateTypeError(
                                     script_state->GetIsolate(),
                                     "At least one property must be provided"));
  }

  if (contact_picker_in_use_) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Contacts Picker is already in use."));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  bool include_names = false;
  bool include_emails = false;
  bool include_tel = false;

  for (const String& property : properties) {
    if (property == "name")
      include_names = true;
    else if (property == "email")
      include_emails = true;
    else if (property == "tel")
      include_tel = true;
  }

  contact_picker_in_use_ = true;
  GetContactsManager(script_state)
      ->Select(options->multiple(), include_names, include_emails, include_tel,
               WTF::Bind(&ContactsManager::OnContactsSelected,
                         WrapPersistent(this), WrapPersistent(resolver)));

  return promise;
}

void ContactsManager::OnContactsSelected(
    ScriptPromiseResolver* resolver,
    base::Optional<Vector<mojom::blink::ContactInfoPtr>> contacts) {
  ScriptState* script_state = resolver->GetScriptState();

  if (!script_state->ContextIsValid()) {
    // This can happen if the page is programmatically redirected while
    // contacts are still being chosen.
    return;
  }

  ScriptState::Scope scope(script_state);

  contact_picker_in_use_ = false;

  if (!contacts.has_value()) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "Unable to open a contact selector"));
    return;
  }

  HeapVector<Member<ContactInfo>> contacts_list;
  for (const auto& contact : *contacts)
    contacts_list.push_back(contact.To<blink::ContactInfo*>());

  resolver->Resolve(contacts_list);
}

}  // namespace blink
