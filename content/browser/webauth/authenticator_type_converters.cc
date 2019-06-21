// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_type_converters.h"

#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"

namespace mojo {

using ::blink::test::mojom::ClientToAuthenticatorProtocol;

// static
::device::ProtocolVersion
TypeConverter<::device::ProtocolVersion, ClientToAuthenticatorProtocol>::
    Convert(const ClientToAuthenticatorProtocol& input) {
  switch (input) {
    case ClientToAuthenticatorProtocol::U2F:
      return ::device::ProtocolVersion::kU2f;
    case ClientToAuthenticatorProtocol::CTAP2:
      return ::device::ProtocolVersion::kCtap2;
  }
  NOTREACHED();
  return ::device::ProtocolVersion::kUnknown;
}

}  // namespace mojo
