// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.logging;

/** Object used to hold content data for logging events. */
public interface ElementLoggingData {
    /**
     * Returns the position of the content in the stream not accounting for header views. The top
     * being 0. Position does not change after initial layout. Specifically the position does not
     * update if dismisses/removes are performed.
     */
    int getPositionInStream();

    /**
     * Gets the time, in seconds from epoch, for when this content was made available on the device.
     * This could be the time for when this content was retrieved from the server or the time the
     * data was pushed to the device.
     */
    long getTimeContentBecameAvailable();

    /**
     * Gets the URI which represents this content. This will normally be the URI which this content
     * links to.
     */
    String getRepresentationUri();
}
