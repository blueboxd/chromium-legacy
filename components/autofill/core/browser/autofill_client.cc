// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_client.h"

#include "components/version_info/channel.h"

namespace autofill {

version_info::Channel AutofillClient::GetChannel() const {
  return version_info::Channel::UNKNOWN;
}

std::string AutofillClient::GetPageLanguage() const {
  return std::string();
}

std::string AutofillClient::GetVariationConfigCountryCode() const {
  return std::string();
}

#if !defined(OS_IOS)
std::unique_ptr<InternalAuthenticator>
AutofillClient::CreateCreditCardInternalAuthenticator(
    content::RenderFrameHost* rfh) {
  return nullptr;
}
#endif

LogManager* AutofillClient::GetLogManager() const {
  return nullptr;
}

}  // namespace autofill
