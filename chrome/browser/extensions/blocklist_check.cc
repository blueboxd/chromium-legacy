// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blocklist_check.h"

#include "base/bind.h"
#include "chrome/browser/extensions/blocklist.h"
#include "extensions/common/extension.h"

namespace extensions {

BlacklistCheck::BlacklistCheck(Blacklist* blacklist,
                               scoped_refptr<const Extension> extension)
    : PreloadCheck(extension), blacklist_(blacklist) {}

BlacklistCheck::~BlacklistCheck() {}

void BlacklistCheck::Start(ResultCallback callback) {
  callback_ = std::move(callback);

  blacklist_->IsBlacklisted(
      extension()->id(),
      base::Bind(&BlacklistCheck::OnBlacklistedStateRetrieved,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BlacklistCheck::OnBlacklistedStateRetrieved(
    BlocklistState blocklist_state) {
  Errors errors;
  if (blocklist_state == BlocklistState::BLOCKLISTED_MALWARE)
    errors.insert(PreloadCheck::BLOCKLISTED_ID);
  else if (blocklist_state == BlocklistState::BLOCKLISTED_UNKNOWN)
    errors.insert(PreloadCheck::BLOCKLISTED_UNKNOWN);
  std::move(callback_).Run(errors);
}

}  // namespace extensions
