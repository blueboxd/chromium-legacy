// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/welcome/welcome_win10_ui.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/welcome/welcome_win10_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// Helper function to check the presence of a key/value inside the query in the
// |url|.
bool UrlContainsKeyValueInQuery(const GURL& url,
                                const std::string& key,
                                const std::string& expected_value) {
  std::string value;
  return net::GetValueForKeyInQuery(url, key, &value) &&
         value == expected_value;
}

// Adds all the needed localized strings to |html_source|, depending on
// the value of |is_first_run|.
void AddLocalizedStrings(content::WebUIDataSource* html_source,
                         bool is_first_run) {
  // Only show the "Welcome to Chrome" text on first run.
  int welcome_header_id = is_first_run
                              ? IDS_WIN10_WELCOME_HEADER
                              : IDS_WIN10_WELCOME_HEADER_AFTER_FIRST_RUN;
  html_source->AddLocalizedString("headerText", welcome_header_id);

  static constexpr LocalizedString kStrings[] = {
      {"continueText", IDS_WIN10_WELCOME_CONTINUE},

      // Default browser strings.
      {"defaultBrowserSubheaderText",
       IDS_WIN10_WELCOME_MAKE_DEFAULT_SUBHEADING},
      {"openSettingsText", IDS_WIN10_WELCOME_OPEN_SETTINGS},
      {"clickEdgeText", IDS_WIN10_WELCOME_CLICK_EDGE},
      {"clickSelectChrome", IDS_WIN10_WELCOME_SELECT},
      {"switchAnywayLabel", IDS_WIN10_WELCOME_SWITCH_ANYWAY_LABEL},
      {"clickSwitchAnywayText", IDS_WIN10_WELCOME_CLICK_SWITCH_ANYWAY},

      // Taskbar pin strings.
      {"pinSubheaderText", IDS_WIN10_WELCOME_PIN_SUBHEADING},
      {"rightClickText", IDS_WIN10_WELCOME_RIGHT_CLICK_TASKBAR},
      {"pinInstructionText", IDS_WIN10_WELCOME_PIN_INSTRUCTION},
      {"pinToTaskbarLabel", IDS_WIN10_WELCOME_PIN_LABEL},
  };
  AddLocalizedStringsBulk(html_source, kStrings, base::size(kStrings));
}

}  // namespace

WelcomeWin10UI::WelcomeWin10UI(content::WebUI* web_ui, const GURL& url)
    : content::WebUIController(web_ui) {
  // Determine which variation to show.
  bool is_first_run = !UrlContainsKeyValueInQuery(url, "text", "faster");

  web_ui->AddMessageHandler(std::make_unique<WelcomeWin10Handler>());

  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(url.host());

  html_source->SetJsonPath("strings.js");

  AddLocalizedStrings(html_source, is_first_run);

  // Controls the accelerated default browser flow experiment.
  html_source->AddBoolean("acceleratedFlowEnabled",
                          base::FeatureList::IsEnabled(
                              features::kWin10AcceleratedDefaultBrowserFlow));

  html_source->AddResourcePath("welcome_win10.css", IDR_WELCOME_WIN10_CSS);
  html_source->AddResourcePath("welcome_win10.js", IDR_WELCOME_WIN10_JS);
  html_source->AddResourcePath("default.webp", IDR_WELCOME_WIN10_DEFAULT_WEBP);
  html_source->AddResourcePath("pin.webp", IDR_WELCOME_WIN10_PIN_WEBP);

  html_source->AddResourcePath("logo-small.png", IDR_PRODUCT_LOGO_64);
  html_source->AddResourcePath("logo-large.png", IDR_PRODUCT_LOGO_128);

  html_source->SetDefaultResource(IDR_WELCOME_WIN10_HTML);

  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

WelcomeWin10UI::~WelcomeWin10UI() = default;
