// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thinwebview;

/** Various constraints associated with the thin webview based on the usage. */
public class ThinWebViewConstraints implements Cloneable {
    /**
     * Whether this view should be placed on the top of the window. Set this to true only when this
     * view will be used on top of another surface view underneath. Note, don't add any views on the
     * top of this surface in the same window, as they would be invisible.
     */
    public boolean zOrderOnTop;

    /**
     * Whether this view will support opacity.
     */
    public boolean supportsOpacity;

    @Override
    public ThinWebViewConstraints clone() {
        ThinWebViewConstraints clone = new ThinWebViewConstraints();
        clone.zOrderOnTop = zOrderOnTop;
        clone.supportsOpacity = supportsOpacity;
        return clone;
    }
}
