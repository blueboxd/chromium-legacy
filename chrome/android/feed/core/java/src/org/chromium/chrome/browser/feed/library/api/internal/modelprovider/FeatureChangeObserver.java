// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.internal.modelprovider;

/**
 * Defines an interface which observes changes features within the stream model. The {@link
 * #onChange(FeatureChange)} is called when a feature changes.
 */
public interface FeatureChangeObserver {
    /** Called when children of the feature or the underlying payload proto instance change. */
    void onChange(FeatureChange change);
}
