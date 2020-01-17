// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_

class Profile;

namespace content {
class WebUIDataSource;
class WebContents;
}  // namespace content

namespace settings {

// Adds the strings needed by OS and Browser settings page to |html_source|
// This function causes |html_source| to expose a strings.js file from its
// source which contains a mapping from string's name to its translated value.
void AddSharedLocalizedStrings(content::WebUIDataSource* html_source,
                               Profile* profile,
                               content::WebContents* web_contents);

// The set of strings used by the A11y subpage that manages accessibility
// features, excluding the top-level settings rows which link to this section.
void AddSharedA11yStrings(content::WebUIDataSource* html_source);

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SHARED_SETTINGS_LOCALIZED_STRINGS_PROVIDER_H_
