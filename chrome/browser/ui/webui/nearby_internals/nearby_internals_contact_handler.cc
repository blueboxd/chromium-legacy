// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_contact_handler.h"

#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/browser/nearby_sharing/logging/proto_to_dictionary_conversion.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service.h"
#include "chrome/browser/nearby_sharing/nearby_sharing_service_factory.h"

namespace {

std::string FormatAsJSON(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

base::Value GetJavascriptTimestamp() {
  return base::Value(base::Time::Now().ToJsTimeIgnoringNull());
}

// Keys in the JSON representation of a contact message
const char kContactMessageTimeKey[] = "time";
const char kContactMessageContactListChangedKey[] = "contactListChanged";
const char kContactMessageContactsAddedToAllowedListKey[] =
    "contactsAddedToAllowlist";
const char kContactMessageContactsRemovedFromAllowedListKey[] =
    "contactsRemovedFromAllowlist";
const char kContactMessageAllowedIdsKey[] = "allowedIds";
const char kContactMessageContactsPassedKey[] = "contactsPassed";
const char kContactMessageContactRecordKey[] = "contactRecords";

// Converts Contact to a raw dictionary value used as a JSON argument to
// JavaScript functions.
base::Value ContactMessageToDictionary(
    bool contacts_list_changed,
    bool contacts_added_to_allowlist,
    bool contacts_removed_from_allowlist,
    const std::set<std::string>& allowed_contact_ids,
    const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
        contacts) {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  dictionary.SetKey(kContactMessageTimeKey, GetJavascriptTimestamp());
  dictionary.SetBoolKey(kContactMessageContactListChangedKey,
                        contacts_list_changed);
  dictionary.SetBoolKey(kContactMessageContactsAddedToAllowedListKey,
                        contacts_added_to_allowlist);
  dictionary.SetBoolKey(kContactMessageContactsRemovedFromAllowedListKey,
                        contacts_removed_from_allowlist);

  base::Value::ListStorage allowed_ids_list;
  allowed_ids_list.reserve(allowed_contact_ids.size());
  for (const auto& contact_id : allowed_contact_ids)
    allowed_ids_list.push_back(base::Value(contact_id));

  dictionary.SetStringKey(kContactMessageAllowedIdsKey,
                          FormatAsJSON(base::Value(std::move(allowed_ids_list))));

  dictionary.SetBoolKey(kContactMessageContactsPassedKey, contacts.has_value());
  if (contacts) {
    base::Value::ListStorage contact_list;
    contact_list.reserve(contacts->size());
    for (const auto& contact : *contacts)
      contact_list.push_back(
          base::Value(ContactRecordToReadableDictionary(contact)));

    dictionary.SetStringKey(kContactMessageContactRecordKey,
                            FormatAsJSON(base::Value(std::move(contact_list))));
  }
  return dictionary;
}

}  // namespace

NearbyInternalsContactHandler::NearbyInternalsContactHandler(
    content::BrowserContext* context)
    : context_(context) {}

NearbyInternalsContactHandler::~NearbyInternalsContactHandler() = default;

void NearbyInternalsContactHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initializeContacts",
      base::BindRepeating(&NearbyInternalsContactHandler::InitializeContents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "downloadContacts",
      base::BindRepeating(
          &NearbyInternalsContactHandler::HandleDownloadContacts,
          base::Unretained(this)));
}

void NearbyInternalsContactHandler::OnJavascriptAllowed() {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    observer_.Add(service_->GetContactManager());
  } else {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsContactHandler::OnJavascriptDisallowed() {
  observer_.RemoveAll();
}

void NearbyInternalsContactHandler::InitializeContents(
    const base::ListValue* args) {
  AllowJavascript();
}

void NearbyInternalsContactHandler::HandleDownloadContacts(
    const base::ListValue* args) {
  NearbySharingService* service_ =
      NearbySharingServiceFactory::GetForBrowserContext(context_);
  if (service_) {
    const bool only_download_if_contacts_changed = args->GetList()[0].GetBool();
    service_->GetContactManager()->DownloadContacts(
        only_download_if_contacts_changed);
  } else {
    NS_LOG(ERROR) << "No NearbyShareService instance to call.";
  }
}

void NearbyInternalsContactHandler::OnContactsUpdated(
    bool contacts_list_changed,
    bool contacts_added_to_allowlist,
    bool contacts_removed_from_allowlist,
    const std::set<std::string>& allowed_contact_ids,
    const base::Optional<std::vector<nearbyshare::proto::ContactRecord>>&
        contacts) {
  FireWebUIListener("contacts-updated",
                    ContactMessageToDictionary(contacts_list_changed,
                                               contacts_added_to_allowlist,
                                               contacts_removed_from_allowlist,
                                               allowed_contact_ids, contacts));
}
