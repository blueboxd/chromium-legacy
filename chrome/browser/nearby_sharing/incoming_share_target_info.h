// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_INCOMING_SHARE_TARGET_INFO_H_
#define CHROME_BROWSER_NEARBY_SHARING_INCOMING_SHARE_TARGET_INFO_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"

class NearbyConnection;

class IncomingShareTargetInfo {
 public:
  IncomingShareTargetInfo();
  ~IncomingShareTargetInfo();

  IncomingShareTargetInfo(const IncomingShareTargetInfo&) = delete;
  IncomingShareTargetInfo& operator=(const IncomingShareTargetInfo&) = delete;

  IncomingShareTargetInfo(IncomingShareTargetInfo&&);
  IncomingShareTargetInfo& operator=(IncomingShareTargetInfo&&);

  void set_endpoint_id(std::string endpoint_id) {
    endpoint_id_ = std::move(endpoint_id);
  }

  const base::Optional<std::string>& endpoint_id() const {
    return endpoint_id_;
  }

  void set_certificate(NearbyShareDecryptedPublicCertificate certificate) {
    certificate_ = std::move(certificate);
  }

  const base::Optional<NearbyShareDecryptedPublicCertificate>& certificate()
      const {
    return certificate_;
  }

  void set_connection(NearbyConnection* connection) {
    connection_ = connection;
  }

  NearbyConnection* connection() const { return connection_; }

  void set_token(std::string token) { token_ = std::move(token); }

  const base::Optional<std::string>& token() const { return token_; }

 private:
  base::Optional<std::string> endpoint_id_;
  base::Optional<NearbyShareDecryptedPublicCertificate> certificate_;
  NearbyConnection* connection_ = nullptr;
  base::Optional<std::string> token_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_INCOMING_SHARE_TARGET_INFO_H_
