// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/**
 * This is a util class for TabUi unit tests.
 */
// TODO(crbug.com/1023701): Generalize all prepareTab method from tab_ui/junit directory.
@SuppressWarnings("ResultOfMethodCallIgnored")
public class TabUiUnitTestUtils {
    public static TabImpl prepareTab() {
        return mock(TabImpl.class);
    }

    public static TabImpl prepareTab(int tabId) {
        TabImpl tab = prepareTab();
        doReturn(tabId).when(tab).getId();
        return tab;
    }

    public static TabImpl prepareTab(int tabId, int rootId) {
        TabImpl tab = prepareTab(tabId);
        doReturn(rootId).when(tab).getRootId();
        return tab;
    }

    public static TabImpl prepareTab(int tabId, int rootId, String visibleUrl) {
        TabImpl tab = prepareTab(tabId, rootId);
        WebContents webContents = mock(WebContents.class);
        GURL gurl = mock(GURL.class);
        doReturn(visibleUrl).when(gurl).getSpec();
        doReturn(gurl).when(webContents).getVisibleUrl();
        doReturn(webContents).when(tab).getWebContents();
        return tab;
    }
}
