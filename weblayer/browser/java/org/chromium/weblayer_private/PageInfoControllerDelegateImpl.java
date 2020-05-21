// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.StrictModeContext;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.page_info.PageInfoControllerDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;
import org.chromium.weblayer_private.interfaces.SiteSettingsIntentHelper;

/**
 * WebLayer's customization of PageInfoControllerDelegate.
 */
public class PageInfoControllerDelegateImpl extends PageInfoControllerDelegate {
    private final Context mContext;
    private final String mProfileName;

    public PageInfoControllerDelegateImpl(Context context, String profileName, GURL url,
            Supplier<ModalDialogManager> modalDialogManager) {
        super(modalDialogManager, new AutocompleteSchemeClassifierImpl(),
                /** vrHandler= */ null,
                /** isSiteSettingsAvailable= */
                UrlConstants.HTTP_SCHEME.equals(url.getScheme())
                        || UrlConstants.HTTPS_SCHEME.equals(url.getScheme()),
                /** cookieControlsShown= */ false);
        mContext = context;
        mProfileName = profileName;
    }

    /**
     * {@inheritDoc}
     */
    @Override
    public void showSiteSettings(String url) {
        Intent intent =
                SiteSettingsIntentHelper.createIntentForSingleWebsite(mContext, mProfileName, url);

        // Disabling StrictMode to avoid violations (https://crbug.com/819410).
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            mContext.startActivity(intent);
        }
    }
}
