// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_internals/nearby_internals_http_handler.h"

#include "base/bind.h"
#include "base/values.h"

namespace {

// This enum class needs to stay in sync with the Rpc definition in
// chrome/browser/resources/nearby_internals/types.js.
enum class Rpc {
  kCertificate = 0,
  kContact = 1,
  kDevice = 2
};

// This enum class needs to stay in sync with the Direction definition in
// chrome/browser/resources/nearby_internals/types.js.
enum class Direction {
  kRequest = 0,
  kResponse = 1
};

}  // namespace

NearbyInternalsHttpHandler::NearbyInternalsHttpHandler() = default;

NearbyInternalsHttpHandler::~NearbyInternalsHttpHandler() = default;

void NearbyInternalsHttpHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize",
      base::BindRepeating(&NearbyInternalsHttpHandler::InitializeContents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateDevice",
      base::BindRepeating(&NearbyInternalsHttpHandler::UpdateDevice,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "listContactPeople",
      base::BindRepeating(&NearbyInternalsHttpHandler::ListContactPeople,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "listPublicCertificates",
      base::BindRepeating(&NearbyInternalsHttpHandler::ListPublicCertificates,
                          base::Unretained(this)));
}

void NearbyInternalsHttpHandler::OnJavascriptAllowed() {}

void NearbyInternalsHttpHandler::OnJavascriptDisallowed() {}

void NearbyInternalsHttpHandler::InitializeContents(
    const base::ListValue* args) {
  AllowJavascript();
}

void NearbyInternalsHttpHandler::UpdateDevice(const base::ListValue* args) {
  // TODO(julietlevesque): Add functionality for Update Device call, which
  // responds to javascript callback from chrome://nearby-internals HTTP
  // Messages tab.
}
void NearbyInternalsHttpHandler::ListPublicCertificates(
    const base::ListValue* args) {
  // TODO(julietlevesque): Add functionality for List Public Certificates call,
  // which responds to javascript callback from chrome://nearby-internals HTTP
  // Messages tab.
}
void NearbyInternalsHttpHandler::ListContactPeople(
    const base::ListValue* args) {
  // TODO(julietlevesque): Add functionality for List ContactPeople call, which
  // responds to javascript callback from chrome://nearby-internals HTTP
  // Messages tab.
}
