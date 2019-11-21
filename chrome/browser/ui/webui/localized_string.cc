// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/localized_string.h"

#include "content/public/browser/web_ui_data_source.h"

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             base::span<const LocalizedString> strings) {
  for (const auto& str : strings)
    html_source->AddLocalizedString(str.name, str.id);
}

void AddLocalizedStringsBulk(content::WebUIDataSource* html_source,
                             const LocalizedString* strings,
                             size_t num_strings) {
  AddLocalizedStringsBulk(html_source, base::make_span(strings, num_strings));
}
