// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.view.View;

import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElementData;

/**
 * Provides custom elements from the host. Host must handle filtering for supported extensions and
 * returning a view.
 */
public interface CustomElementProvider {
    /** Requests that the host create a view based on an extension on CustomElementData. */
    View createCustomElement(CustomElementData customElementData);

    /** Notify the host that Piet is done with and will no longer use this custom element View. */
    void releaseCustomView(View customElementView, CustomElementData customElementData);
}
