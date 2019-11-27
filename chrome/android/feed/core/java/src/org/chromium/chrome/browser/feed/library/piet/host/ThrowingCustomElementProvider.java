// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.piet.host;

import android.view.View;

import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElement;
import org.chromium.components.feed.core.proto.ui.piet.ElementsProto.CustomElementData;

/**
 * {@link CustomElementProvider} for implementations of Piet that do not use {@link CustomElement}.
 */
public class ThrowingCustomElementProvider implements CustomElementProvider {
    public ThrowingCustomElementProvider() {}

    @Override
    public View createCustomElement(CustomElementData customElementData) {
        throw new UnsupportedOperationException(
                "CustomElements are not supported by this implementation!");
    }

    @Override
    public void releaseCustomView(View customElementView, CustomElementData customElementData) {
        throw new UnsupportedOperationException(
                "CustomElements are not supported by this implementation!");
    }
}
