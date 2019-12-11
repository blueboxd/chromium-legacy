// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thinwebview;

/** Various constraints associated with the thin webview based on the usage. */
public class ThinWebViewConstraints implements Cloneable {
    /**
     * Whether this view will support opacity.
     */
    public boolean supportsOpacity;

    @Override
    public ThinWebViewConstraints clone() {
        ThinWebViewConstraints clone = new ThinWebViewConstraints();
        clone.supportsOpacity = supportsOpacity;
        return clone;
    }
}
