// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

import android.view.View;

/** Interface used to show a tooltip to reference a view. */
public interface TooltipApi {
    /**
     * Shows a tooltip to reference a view if the user is eligible to see the tooltip.
     *
     * @param tooltipInfo information used to show this tooltip including placement info.
     * @param view the view the tooltip is centered on.
     * @param tooltipCallback the callback that will be evoked on tooltip events.
     * @return true if the tooltip is actually shown.
     */
    boolean maybeShowHelpUi(TooltipInfo tooltipInfo, View view, TooltipCallbackApi tooltipCallback);
}
