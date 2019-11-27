// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.stream;

import android.support.annotation.IntDef;

/** Interface for callbacks for tooltip events. */
public interface TooltipCallbackApi {
    /** The ways a tooltip can be dismissed. */
    @IntDef({
            TooltipDismissType.UNKNOWN,
            TooltipDismissType.TIMEOUT,
            TooltipDismissType.CLICK,
            TooltipDismissType.CLICK_OUTSIDE,
    })
    @interface TooltipDismissType {
        int UNKNOWN = 0;
        int TIMEOUT = 1;
        int CLICK = 2;
        int CLICK_OUTSIDE = 3;
    }

    /** The callback that will be triggered when the tooltip is shown. */
    void onShow();

    /**
     * The callback that will be triggered when the tooltip is hidden.
     *
     * @param dismissType the type of event that caused the tooltip to be hidden.
     */
    void onHide(@TooltipDismissType int dismissType);
}
