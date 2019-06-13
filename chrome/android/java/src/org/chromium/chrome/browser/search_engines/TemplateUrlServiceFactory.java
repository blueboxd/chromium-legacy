// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * This factory links the native TemplateURLService for the current Profile to create and hold a
 * {@link TemplateUrlService} singleton.
 */
public class TemplateUrlServiceFactory {
    private static TemplateUrlService sTemplateUrlService;

    private TemplateUrlServiceFactory() {}

    /**
     * @return The singleton instance of {@link TemplateUrlService}, creating it if necessary.
     */
    public static TemplateUrlService get() {
        ThreadUtils.assertOnUiThread();
        if (sTemplateUrlService == null) {
            sTemplateUrlService = nativeGetTemplateUrlService();
        }
        return sTemplateUrlService;
    }

    @VisibleForTesting
    static void setInstanceForTesting(TemplateUrlService service) {
        sTemplateUrlService = service;
    }

    /**
     * TODO(crbug.com/968156): Move to TemplateUrlServiceHelper.
     */
    public static boolean doesDefaultSearchEngineHaveLogo() {
        return nativeDoesDefaultSearchEngineHaveLogo();
    }

    private static native TemplateUrlService nativeGetTemplateUrlService();
    private static native boolean nativeDoesDefaultSearchEngineHaveLogo();
}
