// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.header;

import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties associated with the header suggestions. */
public class HeaderViewProperties {
    /** The text content to be displayed as a header text. */
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_UNIQUE_KEYS = new PropertyKey[] {TITLE};

    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(ALL_UNIQUE_KEYS, SuggestionCommonProperties.ALL_KEYS);
}
