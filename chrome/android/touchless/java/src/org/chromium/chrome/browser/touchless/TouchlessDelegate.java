// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touchless;

import android.content.Context;
import android.support.annotation.StringRes;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.native_page.NativePage;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.permissions.PermissionDialogDelegate;
import org.chromium.chrome.browser.touchless.permissions.TouchlessPermissionDialogModel;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Provides an entry point into touchless code from the rest of Chrome.
 */
public class TouchlessDelegate {
    public static final boolean TOUCHLESS_MODE_ENABLED = true;

    public static NativePage createTouchlessNewTabPage(
            ChromeActivity activity, NativePageHost host) {
        return new TouchlessNewTabPage(activity, host);
    }

    public static boolean isTouchlessNewTabPage(NativePage nativePage) {
        return nativePage instanceof TouchlessNewTabPage;
    }

    public static NativePage createTouchlessExploreSitesPage(
            ChromeActivity activity, NativePageHost host) {
        return new TouchlessExploreSitesPage(activity, host);
    }

    public static Class<? extends ChromeActivity> getNoTouchActivityClass() {
        return NoTouchActivity.class;
    }

    public static Class<?> getTouchlessPreferencesClass() {
        return TouchlessPreferences.class;
    }

    public static PropertyModel getPermissionDialogModel(
            ModalDialogProperties.Controller controller, PermissionDialogDelegate delegate) {
        return TouchlessPermissionDialogModel.getModel(controller, delegate);
    }

    public static PropertyModel getMissingPermissionDialogModel(Context context,
            ModalDialogProperties.Controller controller, @StringRes int messageId) {
        return TouchlessPermissionDialogModel.getMissingPermissionDialogModel(
                context, controller, messageId);
    }

    public static TouchlessUiCoordinator getTouchlessUiCoordinator(ChromeActivity activity) {
        return new TouchlessUiCoordinatorImpl(activity);
    }
}
