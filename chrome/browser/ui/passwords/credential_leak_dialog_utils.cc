// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/credential_leak_dialog_utils.h"

#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

using password_manager::CredentialLeakFlags;
using password_manager::CredentialLeakType;

namespace leak_dialog_utils {

base::string16 GetAcceptButtonLabel(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(
      (password_manager::IsPasswordUsedOnOtherSites(leak_type) &&
       password_manager::IsSyncingPasswordsNormally(leak_type))
          ? IDS_LEAK_CHECK_CREDENTIALS
          : IDS_OK);
}

base::string16 GetCloseButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

base::string16 GetDescription(CredentialLeakType leak_type,
                              const GURL& origin) {
  const auto formatted = url_formatter::FormatOriginForSecurityDisplay(
      url::Origin::Create(origin),
      url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  if (!password_manager::IsSyncingPasswordsNormally(leak_type) ||
      !password_manager::IsPasswordUsedOnOtherSites(leak_type)) {
    return l10n_util::GetStringFUTF16(IDS_CREDENTIAL_LEAK_CURRENT_SITE_MESSAGE,
                                      formatted);
  } else if (password_manager::IsPasswordSaved(leak_type)) {
    return l10n_util::GetStringUTF16(
        IDS_CREDENTIAL_LEAK_SAVED_MULTIPLE_SITES_MESSAGE);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CREDENTIAL_LEAK_NOT_SAVED_MULTIPLE_SITES_MESSAGE, formatted);
  }
}

base::string16 GetTitle(CredentialLeakType leak_type) {
  // We don't check sync state here. For users that are not syncing their
  // passwords or syncing with custom passphrase the title can be "Check your
  // passwords", but there is no "Check passwords" button.
  return l10n_util::GetStringUTF16(
      password_manager::IsPasswordUsedOnOtherSites(leak_type)
          ? IDS_CREDENTIAL_LEAK_MULTIPLE_SITES_TITLE
          : IDS_CREDENTIAL_LEAK_CURRENT_SITE_TITLE);
}

bool ShouldCheckPasswords(CredentialLeakType leak_type) {
  return password_manager::IsPasswordUsedOnOtherSites(leak_type) &&
         password_manager::IsSyncingPasswordsNormally(leak_type);
}

bool ShouldShowCloseButton(CredentialLeakType leak_type) {
  return ShouldCheckPasswords(leak_type);
}

}  // namespace leak_dialog_utils
