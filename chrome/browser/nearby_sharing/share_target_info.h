// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_INFO_H_
#define CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_INFO_H_

#include <memory>
#include <string>

#include "base/optional.h"
#include "chrome/browser/nearby_sharing/certificates/nearby_share_decrypted_public_certificate.h"
#include "chrome/browser/nearby_sharing/incoming_frames_reader.h"
#include "chrome/browser/nearby_sharing/paired_key_verification_runner.h"

class NearbyConnection;

// Additional information about the connection to a remote device.
class ShareTargetInfo {
 public:
  ShareTargetInfo();
  ShareTargetInfo(ShareTargetInfo&&);
  ShareTargetInfo& operator=(ShareTargetInfo&&);
  virtual ~ShareTargetInfo();

  const base::Optional<std::string>& endpoint_id() const {
    return endpoint_id_;
  }

  void set_endpoint_id(std::string endpoint_id) {
    endpoint_id_ = std::move(endpoint_id);
  }

  const base::Optional<NearbyShareDecryptedPublicCertificate>& certificate()
      const {
    return certificate_;
  }

  void set_certificate(NearbyShareDecryptedPublicCertificate certificate) {
    certificate_ = std::move(certificate);
  }

  NearbyConnection* connection() const { return connection_; }

  void set_connection(NearbyConnection* connection) {
    connection_ = connection;
  }

  const base::Optional<std::string>& token() const { return token_; }

  void set_token(std::string token) { token_ = std::move(token); }

  IncomingFramesReader* frames_reader() const { return frames_reader_.get(); }

  void set_frames_reader(std::unique_ptr<IncomingFramesReader> frames_reader) {
    frames_reader_ = std::move(frames_reader);
  }

  PairedKeyVerificationRunner* key_verification_runner() {
    return key_verification_runner_.get();
  }

  void set_key_verification_runner(
      std::unique_ptr<PairedKeyVerificationRunner> key_verification_runner) {
    key_verification_runner_ = std::move(key_verification_runner);
  }

 private:
  base::Optional<std::string> endpoint_id_;
  base::Optional<NearbyShareDecryptedPublicCertificate> certificate_;
  NearbyConnection* connection_ = nullptr;
  base::Optional<std::string> token_;
  std::unique_ptr<IncomingFramesReader> frames_reader_;
  std::unique_ptr<PairedKeyVerificationRunner> key_verification_runner_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_SHARE_TARGET_INFO_H_
