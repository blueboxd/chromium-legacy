// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_cast_private/networking_cast_private_api.h"

#include <utility>

#include "base/bind.h"
#include "chrome/common/extensions/api/networking_cast_private.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/networking_private/networking_cast_private_delegate.h"

namespace private_api = extensions::api::networking_private;
namespace cast_api = extensions::api::networking_cast_private;

namespace extensions {

namespace {

std::unique_ptr<NetworkingCastPrivateDelegate::Credentials> AsCastCredentials(
    api::networking_cast_private::VerificationProperties& properties) {
  return std::make_unique<NetworkingCastPrivateDelegate::Credentials>(
      properties.certificate,
      properties.intermediate_certificates
          ? *properties.intermediate_certificates
          : std::vector<std::string>(),
      properties.signed_data, properties.device_ssid, properties.device_serial,
      properties.device_bssid, properties.public_key, properties.nonce);
}

}  // namespace

NetworkingCastPrivateVerifyDestinationFunction::
    ~NetworkingCastPrivateVerifyDestinationFunction() {}

ExtensionFunction::ResponseAction
NetworkingCastPrivateVerifyDestinationFunction::Run() {
  std::unique_ptr<cast_api::VerifyDestination::Params> params =
      cast_api::VerifyDestination::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkingCastPrivateDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNetworkingCastPrivateDelegate();
  delegate->VerifyDestination(
      AsCastCredentials(params->properties),
      base::BindOnce(&NetworkingCastPrivateVerifyDestinationFunction::Success,
                     this),
      base::BindOnce(&NetworkingCastPrivateVerifyDestinationFunction::Failure,
                     this));

  // VerifyDestination might respond synchronously, e.g. in tests.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingCastPrivateVerifyDestinationFunction::Success(bool result) {
  Respond(ArgumentList(cast_api::VerifyDestination::Results::Create(result)));
}

void NetworkingCastPrivateVerifyDestinationFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

NetworkingCastPrivateVerifyAndEncryptDataFunction::
    ~NetworkingCastPrivateVerifyAndEncryptDataFunction() {}

ExtensionFunction::ResponseAction
NetworkingCastPrivateVerifyAndEncryptDataFunction::Run() {
  std::unique_ptr<cast_api::VerifyAndEncryptData::Params> params =
      cast_api::VerifyAndEncryptData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  NetworkingCastPrivateDelegate* delegate =
      ExtensionsAPIClient::Get()->GetNetworkingCastPrivateDelegate();
  delegate->VerifyAndEncryptData(
      params->data, AsCastCredentials(params->properties),
      base::BindOnce(
          &NetworkingCastPrivateVerifyAndEncryptDataFunction::Success, this),
      base::BindOnce(
          &NetworkingCastPrivateVerifyAndEncryptDataFunction::Failure, this));

  // VerifyAndEncryptData might respond synchronously, e.g. in tests.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void NetworkingCastPrivateVerifyAndEncryptDataFunction::Success(
    const std::string& result) {
  Respond(
      ArgumentList(cast_api::VerifyAndEncryptData::Results::Create(result)));
}

void NetworkingCastPrivateVerifyAndEncryptDataFunction::Failure(
    const std::string& error) {
  Respond(Error(error));
}

NetworkingCastPrivateSetWifiTDLSEnabledStateFunction::
    ~NetworkingCastPrivateSetWifiTDLSEnabledStateFunction() {}

ExtensionFunction::ResponseAction
NetworkingCastPrivateSetWifiTDLSEnabledStateFunction::Run() {
  std::unique_ptr<cast_api::SetWifiTDLSEnabledState::Params> params =
      cast_api::SetWifiTDLSEnabledState::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error("Not supported"));
}

NetworkingCastPrivateGetWifiTDLSStatusFunction::
    ~NetworkingCastPrivateGetWifiTDLSStatusFunction() {}

ExtensionFunction::ResponseAction
NetworkingCastPrivateGetWifiTDLSStatusFunction::Run() {
  std::unique_ptr<cast_api::GetWifiTDLSStatus::Params> params =
      cast_api::GetWifiTDLSStatus::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  return RespondNow(Error("Not supported"));
}

}  // namespace extensions
