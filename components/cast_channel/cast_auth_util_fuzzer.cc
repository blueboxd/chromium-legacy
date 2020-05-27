// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <iostream>

#include "components/cast_channel/cast_auth_util.h"
#include "components/cast_channel/fuzz_proto/fuzzer_inputs.pb.h"
#include "net/cert/x509_certificate.h"
#include "testing/libfuzzer/proto/lpm_interface.h"

namespace cast_channel {
namespace fuzz {

namespace {
const char kCertData[] = {
#include "net/data/ssl/certificates/wildcard.inc"
};
}  // namespace

DEFINE_PROTO_FUZZER(const CastAuthUtilInputs& input_union) {
  // TODO(crbug.com/796717): Add tests for AuthenticateChallengeReply and
  // VerifyTLSCertificate if necessary.  Refer to updates on the bug, and check
  // to see if there is already coverage through BoringSSL

  switch (input_union.input_case()) {
    case CastAuthUtilInputs::kAuthenticateChallengeReplyInput: {
      const auto& input = input_union.authenticate_challenge_reply_input();
      cast::channel::DeviceAuthMessage auth_message = input.auth_message();
      AuthContext context = AuthContext::CreateForTest(input.nonce());
      scoped_refptr<net::X509Certificate> peer_cert =
          net::X509Certificate::CreateFromBytes(kCertData,
                                                base::size(kCertData));
      AuthenticateChallengeReply(input.cast_message(), *peer_cert, context);
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace fuzz
}  // namespace cast_channel
