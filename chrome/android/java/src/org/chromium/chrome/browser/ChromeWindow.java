// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/**
 * The window that has access to the main activity and is able to create and receive intents,
 * and show error messages.
 */
public class ChromeWindow extends ActivityWindowAndroid {
    /**
     * Interface allowing to inject a different keyboard delegate for testing.
     */
    @VisibleForTesting
    public interface KeyboardVisibilityDelegateFactory {
        ActivityKeyboardVisibilityDelegate create(WeakReference<Activity> activity);
    }
    private static KeyboardVisibilityDelegateFactory sKeyboardVisibilityDelegateFactory =
            ChromeKeyboardVisibilityDelegate::new;

    /**
     * Creates Chrome specific ActivityWindowAndroid.
     * @param activity The activity that owns the ChromeWindow.
     */
    public ChromeWindow(ChromeActivity activity) {
        super(activity);
    }

    @Override
    public View getReadbackView() {
        assert getActivity().get() instanceof ChromeActivity;

        ChromeActivity chromeActivity = (ChromeActivity) getActivity().get();
        return chromeActivity.getCompositorViewHolder() == null
                ? null
                : chromeActivity.getCompositorViewHolder().getActiveSurfaceView();
    }

    @Override
    public ModalDialogManager getModalDialogManager() {
        ChromeActivity activity = (ChromeActivity) getActivity().get();
        return activity == null ? null : activity.getModalDialogManager();
    }

    @Override
    protected ActivityKeyboardVisibilityDelegate createKeyboardVisibilityDelegate() {
        return sKeyboardVisibilityDelegateFactory.create(getActivity());
    }

    /**
     * Shows an infobar error message overriding the WindowAndroid implementation.
     */
    @Override
    protected void showCallbackNonExistentError(String error) {
        Activity activity = getActivity().get();

        // We can assume that activity is a ChromeActivity because we require one to be passed in
        // in the constructor.
        Tab tab = activity != null ? ((ChromeActivity) activity).getActivityTab() : null;

        if (tab != null) {
            SimpleConfirmInfoBarBuilder.create(tab.getWebContents(),
                    InfoBarIdentifier.WINDOW_ERROR_INFOBAR_DELEGATE_ANDROID, error, false);
        } else {
            super.showCallbackNonExistentError(error);
        }
    }

    @VisibleForTesting
    public static void setKeyboardVisibilityDelegateFactory(
            KeyboardVisibilityDelegateFactory factory) {
        sKeyboardVisibilityDelegateFactory = factory;
    }

    @VisibleForTesting
    public static void resetKeyboardVisibilityDelegateFactory() {
        setKeyboardVisibilityDelegateFactory(ChromeKeyboardVisibilityDelegate::new);
    }
}
