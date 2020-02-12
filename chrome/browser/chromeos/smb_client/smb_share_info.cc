// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_share_info.h"

#include "base/logging.h"

namespace chromeos {
namespace smb_client {

SmbShareInfo::SmbShareInfo(const SmbUrl& share_url,
                           const std::string& display_name,
                           const std::string& username,
                           const std::string& workgroup,
                           bool use_kerberos)
    : share_url_(share_url),
      display_name_(display_name),
      username_(username),
      workgroup_(workgroup),
      use_kerberos_(use_kerberos) {
  DCHECK(share_url_.IsValid());
  DCHECK(!display_name.empty());
}

SmbShareInfo::~SmbShareInfo() = default;

SmbShareInfo::SmbShareInfo(const SmbShareInfo&) = default;

SmbShareInfo& SmbShareInfo::operator=(const SmbShareInfo&) = default;

}  // namespace smb_client
}  // namespace chromeos
