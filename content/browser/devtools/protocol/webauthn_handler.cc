// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/webauthn_handler.h"

#include <map>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_u2f_device.h"

namespace content {
namespace protocol {

namespace {
static constexpr char kAuthenticatorNotFound[] =
    "Could not find a Virtual Authenticator matching the ID";
static constexpr char kCableNotSupportedOnU2f[] =
    "U2F only supports the \"usb\", \"ble\" and \"nfc\" transports";
static constexpr char kCouldNotCreateCredential[] =
    "An error occurred trying to create the credential";
static constexpr char kDevToolsNotAttached[] =
    "The DevTools session is not attached to a frame";
static constexpr char kErrorCreatingAuthenticator[] =
    "An error occurred when trying to create the authenticator";
static constexpr char kInvalidProtocol[] = "The protocol is not valid";
static constexpr char kInvalidRpIdHash[] =
    "The Relying Party ID hash must have a size of ";
static constexpr char kInvalidTransport[] = "The transport is not valid";
static constexpr char kVirtualEnvironmentNotEnabled[] =
    "The Virtual Authenticator Environment has not been enabled for this "
    "session";

device::ProtocolVersion ConvertToProtocolVersion(base::StringPiece protocol) {
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::Ctap2)
    return device::ProtocolVersion::kCtap2;
  if (protocol == WebAuthn::AuthenticatorProtocolEnum::U2f)
    return device::ProtocolVersion::kU2f;
  return device::ProtocolVersion::kUnknown;
}

std::vector<uint8_t> CopyBinaryToVector(const Binary& binary) {
  return std::vector<uint8_t>(binary.data(), binary.data() + binary.size());
}

}  // namespace

WebAuthnHandler::WebAuthnHandler()
    : DevToolsDomainHandler(WebAuthn::Metainfo::domainName) {}

WebAuthnHandler::~WebAuthnHandler() = default;

void WebAuthnHandler::SetRenderer(int process_host_id,
                                  RenderFrameHostImpl* frame_host) {
  if (!frame_host) {
    Disable();
  }
  frame_host_ = frame_host;
}

void WebAuthnHandler::Wire(UberDispatcher* dispatcher) {
  WebAuthn::Dispatcher::wire(dispatcher, this);
}

Response WebAuthnHandler::Enable() {
  if (!frame_host_)
    return Response::Error(kDevToolsNotAttached);

  AuthenticatorEnvironmentImpl::GetInstance()->EnableVirtualAuthenticatorFor(
      frame_host_->frame_tree_node());
  virtual_discovery_factory_ =
      AuthenticatorEnvironmentImpl::GetInstance()->GetVirtualFactoryFor(
          frame_host_->frame_tree_node());
  return Response::OK();
}

Response WebAuthnHandler::Disable() {
  if (frame_host_) {
    AuthenticatorEnvironmentImpl::GetInstance()->DisableVirtualAuthenticatorFor(
        frame_host_->frame_tree_node());
  }
  virtual_discovery_factory_ = nullptr;
  return Response::OK();
}

Response WebAuthnHandler::AddVirtualAuthenticator(
    std::unique_ptr<WebAuthn::VirtualAuthenticatorOptions> options,
    String* out_authenticator_id) {
  if (!virtual_discovery_factory_)
    return Response::Error(kVirtualEnvironmentNotEnabled);

  auto transport =
      device::ConvertToFidoTransportProtocol(options->GetTransport());
  if (!transport)
    return Response::InvalidParams(kInvalidTransport);

  auto protocol = ConvertToProtocolVersion(options->GetProtocol());
  if (protocol == device::ProtocolVersion::kUnknown)
    return Response::InvalidParams(kInvalidProtocol);

  if (protocol == device::ProtocolVersion::kU2f &&
      !device::VirtualU2fDevice::IsTransportSupported(*transport)) {
    return Response::InvalidParams(kCableNotSupportedOnU2f);
  }

  auto* authenticator = virtual_discovery_factory_->CreateAuthenticator(
      protocol, *transport,
      transport == device::FidoTransportProtocol::kInternal
          ? device::AuthenticatorAttachment::kPlatform
          : device::AuthenticatorAttachment::kCrossPlatform,
      options->GetHasResidentKey(), options->GetHasUserVerification());
  if (!authenticator)
    return Response::Error(kErrorCreatingAuthenticator);

  authenticator->SetUserPresence(
      options->GetAutomaticPresenceSimulation(true /* default */));

  *out_authenticator_id = authenticator->unique_id();
  return Response::OK();
}

Response WebAuthnHandler::RemoveVirtualAuthenticator(
    const String& authenticator_id) {
  if (!virtual_discovery_factory_)
    return Response::Error(kVirtualEnvironmentNotEnabled);

  if (!virtual_discovery_factory_->RemoveAuthenticator(authenticator_id))
    return Response::InvalidParams(kAuthenticatorNotFound);

  return Response::OK();
}

Response WebAuthnHandler::AddCredential(
    const String& authenticator_id,
    std::unique_ptr<WebAuthn::Credential> credential) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.isSuccess())
    return response;

  if (credential->GetRpIdHash().size() != device::kRpIdHashLength) {
    return Response::InvalidParams(
        kInvalidRpIdHash + base::NumberToString(device::kRpIdHashLength));
  }

  if (!authenticator->AddRegistration(
          CopyBinaryToVector(credential->GetCredentialId()),
          CopyBinaryToVector(credential->GetRpIdHash()),
          CopyBinaryToVector(credential->GetPrivateKey()),
          credential->GetSignCount())) {
    return Response::Error(kCouldNotCreateCredential);
  }

  return Response::OK();
}

Response WebAuthnHandler::GetCredentials(
    const String& authenticator_id,
    std::unique_ptr<Array<WebAuthn::Credential>>* out_credentials) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.isSuccess())
    return response;

  *out_credentials = std::make_unique<Array<WebAuthn::Credential>>();
  for (const auto& credential : authenticator->registrations()) {
    const auto& rp_id_hash = credential.second.application_parameter;
    std::vector<uint8_t> private_key;
    credential.second.private_key->ExportPrivateKey(&private_key);
    (*out_credentials)
        ->emplace_back(
            WebAuthn::Credential::Create()
                .SetCredentialId(Binary::fromVector(credential.first))
                .SetRpIdHash(
                    Binary::fromSpan(rp_id_hash.data(), rp_id_hash.size()))
                .SetPrivateKey(Binary::fromVector(std::move(private_key)))
                .SetSignCount(credential.second.counter)
                .Build());
  }
  return Response::OK();
}

Response WebAuthnHandler::ClearCredentials(const String& authenticator_id) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.isSuccess())
    return response;

  authenticator->ClearRegistrations();
  return Response::OK();
}

Response WebAuthnHandler::SetUserVerified(const String& authenticator_id,
                                          bool is_user_verified) {
  VirtualAuthenticator* authenticator;
  Response response = FindAuthenticator(authenticator_id, &authenticator);
  if (!response.isSuccess())
    return response;

  authenticator->set_user_verified(is_user_verified);
  return Response::OK();
}

Response WebAuthnHandler::FindAuthenticator(
    const String& id,
    VirtualAuthenticator** out_authenticator) {
  *out_authenticator = nullptr;
  if (!virtual_discovery_factory_)
    return Response::Error(kVirtualEnvironmentNotEnabled);

  *out_authenticator = virtual_discovery_factory_->GetAuthenticator(id);
  if (!*out_authenticator)
    return Response::InvalidParams(kAuthenticatorNotFound);

  return Response::OK();
}

}  // namespace protocol
}  // namespace content
