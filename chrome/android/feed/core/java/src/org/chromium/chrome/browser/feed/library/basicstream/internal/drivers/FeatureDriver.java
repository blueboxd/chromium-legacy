// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.basicstream.internal.drivers;

/**
 * A FeatureDriver is an object which can generate a {@link LeafFeatureDriver} from a {@link
 * org.chromium.chrome.browser.feed.library.api.internal.modelprovider.ModelFeature}.
 */
public interface FeatureDriver {
    void onDestroy();

    /*@Nullable*/
    LeafFeatureDriver getLeafFeatureDriver();
}
