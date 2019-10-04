// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/external_protocol_dialog_delegate.h"

#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

namespace {

const size_t kMaxCommandSize = 32;

base::string16 ElideCommandName(const base::string16& command_name) {
  base::string16 elided_command;
  gfx::ElideString(command_name, kMaxCommandSize, &elided_command);
  return elided_command;
}

}  // namespace

ExternalProtocolDialogDelegate::ExternalProtocolDialogDelegate(
    const GURL& url,
    content::WebContents* web_contents,
    const base::Optional<url::Origin>& initiating_origin)
    : ProtocolDialogDelegate(url),
      content::WebContentsObserver(web_contents),
      program_name_(shell_integration::GetApplicationNameForProtocol(url)),
      initiating_origin_(initiating_origin) {}

ExternalProtocolDialogDelegate::~ExternalProtocolDialogDelegate() {}

base::string16 ExternalProtocolDialogDelegate::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return l10n_util::GetStringFUTF16(IDS_EXTERNAL_PROTOCOL_OK_BUTTON_TEXT,
                                      ElideCommandName(program_name_));
  }
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CANCEL_BUTTON_TEXT);
}

base::string16 ExternalProtocolDialogDelegate::GetMessageText() const {
  if (!initiating_origin_ || initiating_origin_->opaque())
    return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_MESSAGE);
  return l10n_util::GetStringFUTF16(
      IDS_EXTERNAL_PROTOCOL_MESSAGE_WITH_INITIATING_ORIGIN,
      url_formatter::FormatOriginForSecurityDisplay(*initiating_origin_));
}

base::string16 ExternalProtocolDialogDelegate::GetCheckboxText() const {
  return l10n_util::GetStringUTF16(IDS_EXTERNAL_PROTOCOL_CHECKBOX_TEXT);
}

base::string16 ExternalProtocolDialogDelegate::GetTitleText() const {
  return l10n_util::GetStringFUTF16(IDS_EXTERNAL_PROTOCOL_TITLE,
                                    ElideCommandName(program_name_));
}

void ExternalProtocolDialogDelegate::DoAccept(const GURL& url,
                                              bool remember) const {
  if (!web_contents())
    return;  // The dialog may outlast the WebContents.

  if (remember) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents()->GetBrowserContext());

    ExternalProtocolHandler::SetBlockState(
        url.scheme(), ExternalProtocolHandler::DONT_BLOCK, profile);
  }

  ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck(url, web_contents());
}
