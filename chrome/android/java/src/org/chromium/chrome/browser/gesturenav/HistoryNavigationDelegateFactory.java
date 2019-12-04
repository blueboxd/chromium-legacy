// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.graphics.Insets;
import android.os.Build;
import android.view.View;
import android.view.WindowInsets;

import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * Factory class for {@link HistoryNavigationDelegate}.
 */
public class HistoryNavigationDelegateFactory {
    /**
     * Default {@link HistoryNavigationDelegate} that does not support navigation. Mainly
     * used for SnackbarActivity-based UI on the phone to disable the feature.
     */
    public static final HistoryNavigationDelegate DEFAULT = new HistoryNavigationDelegate() {
        @Override
        public NavigationHandler.ActionDelegate createActionDelegate() {
            return null;
        }

        @Override
        public NavigationSheet.Delegate createSheetDelegate() {
            return null;
        }

        @Override
        public Supplier<BottomSheetController> getBottomSheetController() {
            assert false : "Should never be called";
            return null;
        }

        @Override
        public boolean isNavigationEnabled(View view) {
            return false;
        }

        @Override
        public void setWindowInsetsChangeObserver(View view, Runnable runnable) {}
    };

    private HistoryNavigationDelegateFactory() {}

    /**
     * Creates {@link HistoryNavigationDelegate} for native/rendered pages on Tab.
     */
    public static HistoryNavigationDelegate create(Tab tab) {
        if (!isFeatureFlagEnabled() || ((TabImpl) tab).getActivity() == null) return DEFAULT;

        return new HistoryNavigationDelegate() {
            // TODO(jinsukkim): Avoid getting activity from tab.
            private final Supplier<BottomSheetController> mController = () -> {
                ChromeActivity activity = ((TabImpl) tab).getActivity();
                return activity == null || activity.isActivityFinishingOrDestroyed()
                        ? null
                        : activity.getBottomSheetController();
            };
            private Runnable mInsetsChangeRunnable;

            @Override
            public NavigationHandler.ActionDelegate createActionDelegate() {
                return new TabbedActionDelegate(tab);
            }

            @Override
            public NavigationSheet.Delegate createSheetDelegate() {
                return new TabbedSheetDelegate(tab);
            }

            @Override
            public Supplier<BottomSheetController> getBottomSheetController() {
                return mController;
            }

            @Override
            public boolean isNavigationEnabled(View view) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return true;
                Insets insets = view.getRootWindowInsets().getSystemGestureInsets();
                return insets.left == 0 && insets.right == 0;
            }

            @Override
            public void setWindowInsetsChangeObserver(View view, Runnable runnable) {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return;
                mInsetsChangeRunnable = runnable;
                view.setOnApplyWindowInsetsListener(
                        runnable != null ? this::onApplyWindowInsets : null);
            }

            private WindowInsets onApplyWindowInsets(View view, WindowInsets insets) {
                assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
                mInsetsChangeRunnable.run();
                return insets;
            }
        };
    }

    private static boolean isFeatureFlagEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.OVERSCROLL_HISTORY_NAVIGATION);
    }
}
