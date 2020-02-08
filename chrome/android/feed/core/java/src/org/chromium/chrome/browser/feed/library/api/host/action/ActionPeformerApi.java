// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.api.host.action;

import org.chromium.chrome.browser.feed.library.api.client.knowncontent.ContentMetadata;

/**
 * Allows the Feed instruct the host to perform actions. In the case that an action is triggered
 * that is not enabled the host has one of two options:
 *
 * <ol>
 *   <li>The host can ignore the action.
 *   <li>The host can throw an exception which the Stream will not catch, crashing the app.
 * </ol>
 */
public interface ActionPeformerApi {
    /** Opens the given URL. */
    void openUrl(String url);

    /** Opens the given URL in incognito mode. */
    void openUrlInIncognitoMode(String url);

    /** Opens the given URL in a new tab. */
    void openUrlInNewTab(String url);

    /** Opens the given URL in a new window. */
    void openUrlInNewWindow(String url);

    /**
     * Downloads the given url.
     *
     * @param contentMetadata The {@link ContentMetadata} defining the content to be downloaded.
     */
    void downloadUrl(ContentMetadata contentMetadata);

    /** Opens the SendFeedback dialog for the user to report problems. */
    void sendFeedback(ContentMetadata contentMetadata);

    /** Opens the Google Product Help page for the Feed. */
    void learnMore();
}
