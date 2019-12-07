// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

import org.chromium.base.Consumer;
import org.chromium.chrome.browser.feed.library.api.host.stream.TooltipInfo.FeatureName;

/** Interface to communicate if particular types of tooltips meet triggering conditions. */
public interface TooltipSupportedApi {
    /**
     * Checks if the tooltip would be shown if {@link TooltipApi#maybeShowHelpUi} is called.
     *
     * @param featureName the name of the tooltip feature that might be triggered.
     * @param consumer a callback that contains a boolean indicating that the tooltip would be shown
     *     if {@link TooltipApi#maybeShowHelpUi} is called.
     */
    void wouldTriggerHelpUi(@FeatureName String featureName, Consumer<Boolean> consumer);
}
