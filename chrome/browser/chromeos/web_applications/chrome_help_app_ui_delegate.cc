// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/chrome_help_app_ui_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "url/gurl.h"

ChromeHelpAppUIDelegate::ChromeHelpAppUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

base::Optional<std::string> ChromeHelpAppUIDelegate::OpenFeedbackDialog() {
  Profile* profile = Profile::FromWebUI(web_ui_);
  // TODO(crbug/1045222): Additional strings are blank right now while we decide
  // on the language and relevant information we want feedback to include.
  // Note that category_tag is the name of the listnr bucket we want our
  // reports to end up in. I.e DESKTOP_TAB_GROUPS.
  chrome::ShowFeedbackPage(
      GURL(chromeos::kChromeUIHelpAppURL), profile,
      chrome::kFeedbackSourceHelpApp, std::string() /* description_template */,
      std::string() /* description_placeholder_text */,
      std::string() /* category_tag */, std::string() /* extra_diagnostics */);
  return base::nullopt;
}
