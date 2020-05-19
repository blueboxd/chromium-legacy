// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/settings_overridden_params_providers.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/ntp_overridden_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_bubble_delegate.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings_overridden_params {

base::Optional<ExtensionSettingsOverriddenDialog::Params>
GetNtpOverriddenParams(Profile* profile) {
  const extensions::Extension* extension =
      ExtensionWebUI::GetExtensionControllingURL(
          GURL(chrome::kChromeUINewTabURL), profile);
  if (!extension)
    return base::nullopt;

  // We deliberately re-use the same preference that the bubble UI uses. This
  // way, users won't see the bubble or dialog UI if they've already
  // acknowledged either version.
  const char* preference_name =
      extensions::NtpOverriddenBubbleDelegate::kNtpBubbleAcknowledged;

  constexpr char kHistogramName[] =
      "Extensions.SettingsOverridden.GenericNtpOverriddenDialogResult";

  // TODO(devlin): Adjust these messages based on the previous NTP.
  base::string16 dialog_title = l10n_util::GetStringUTF16(
      IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_TITLE_GENERIC);
  base::string16 dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_NTP_OVERRIDDEN_DIALOG_BODY_GENERIC,
      base::UTF8ToUTF16(extension->name().c_str()));

  return ExtensionSettingsOverriddenDialog::Params(
      extension->id(), preference_name, kHistogramName, std::move(dialog_title),
      std::move(dialog_message));
}

base::Optional<ExtensionSettingsOverriddenDialog::Params>
GetSearchOverriddenParams(Profile* profile) {
  const extensions::Extension* extension =
      extensions::GetExtensionOverridingSearchEngine(profile);
  if (!extension)
    return base::nullopt;

  // We deliberately re-use the same preference that the bubble UI uses. This
  // way, users won't see the bubble or dialog UI if they've already
  // acknowledged either version.
  const char* preference_name =
      extensions::SettingsApiBubbleDelegate::kAcknowledgedPreference;

  constexpr char kHistogramName[] =
      "Extensions.SettingsOverridden.GenericSearchOverriddenDialogResult";

  // Find the active search engine (which is provided by the extension).
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile);
  DCHECK(template_url_service->IsExtensionControlledDefaultSearch());
  const TemplateURL* default_search =
      template_url_service->GetDefaultSearchProvider();
  DCHECK(default_search);
  DCHECK_EQ(TemplateURL::NORMAL_CONTROLLED_BY_EXTENSION,
            default_search->type());

  // NOTE: For most TemplateURLs, there's no guarantee that search_url is a
  // valid URL (it could contain placeholders, etc). However, for extension-
  // provided search engines, we require they be valid URLs.
  GURL search_url(default_search->url());
  DCHECK(search_url.is_valid()) << default_search->url();

  // Format the URL for display.
  const url_formatter::FormatUrlTypes kFormatRules =
      url_formatter::kFormatUrlOmitTrivialSubdomains |
      url_formatter::kFormatUrlTrimAfterHost |
      url_formatter::kFormatUrlOmitHTTPS;
  base::string16 formatted_search_url = url_formatter::FormatUrl(
      search_url, kFormatRules, net::UnescapeRule::SPACES, nullptr, nullptr,
      nullptr);

  // TODO(devlin): Adjust these strings based on the previous search engine.
  base::string16 dialog_title = l10n_util::GetStringUTF16(
      IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_TITLE_GENERIC);
  base::string16 dialog_message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_SEARCH_OVERRIDDEN_DIALOG_BODY_GENERIC, formatted_search_url,
      base::UTF8ToUTF16(extension->name().c_str()));

  return ExtensionSettingsOverriddenDialog::Params(
      extension->id(), preference_name, kHistogramName, std::move(dialog_title),
      std::move(dialog_message));
}

}  // namespace settings_overridden_params
