// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/tls_deprecation_config.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ssl/tls_deprecation_config.pb.h"
#include "crypto/sha2.h"
#include "url/gurl.h"

namespace {

class TLSDeprecationConfigSingleton {
 public:
  void SetProto(
      std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto) {
    proto_ = std::move(proto);
  }

  chrome_browser_ssl::LegacyTLSExperimentConfig* GetProto() const {
    return proto_.get();
  }

  static TLSDeprecationConfigSingleton& GetInstance() {
    static base::NoDestructor<TLSDeprecationConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto_;
};

}  // namespace

void SetRemoteTLSDeprecationConfigProto(
    std::unique_ptr<chrome_browser_ssl::LegacyTLSExperimentConfig> proto) {
  TLSDeprecationConfigSingleton::GetInstance().SetProto(std::move(proto));
}

bool IsTLSDeprecationConfigControlSite(const GURL& url) {
  if (!url.has_host() || !url.SchemeIsCryptographic())
    return false;

  auto* proto = TLSDeprecationConfigSingleton::GetInstance().GetProto();
  if (!proto)
    return false;

  std::string host_hash = crypto::SHA256HashString(url.host_piece());
  const auto& control_site_hashes = proto->control_site_hashes();

  // Perform binary search on the sorted list of control site hashes to check
  // if the input URL's hostname is included.
  auto lower = std::lower_bound(control_site_hashes.begin(),
                                control_site_hashes.end(), host_hash);

  return lower != control_site_hashes.end() && *lower == host_hash;
}
