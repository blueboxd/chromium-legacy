// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.assertion.ViewAssertions.matches;

import android.graphics.Bitmap;
import android.support.annotation.Nullable;
import android.support.design.widget.CoordinatorLayout;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingViewException;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.image_fetcher.ImageFetcherConfig;
import org.chromium.chrome.browser.snackbar.BottomContainer;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheet;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * Contains utilities for testing Autofill Assistant.
 */
class AutofillAssistantUiTestUtil {
    /** Image fetcher which synchronously returns a preset image. */
    static class MockImageFetcher extends ImageFetcher {
        private final Bitmap mBitmapToFetch;
        private final BaseGifImage mGifToFetch;

        MockImageFetcher(@Nullable Bitmap bitmapToFetch, @Nullable BaseGifImage gifToFetch) {
            mBitmapToFetch = bitmapToFetch;
            mGifToFetch = gifToFetch;
        }

        @Override
        public void fetchGif(String url, String clientName, Callback<BaseGifImage> callback) {
            callback.onResult(mGifToFetch);
        }

        @Override
        public void fetchImage(
                String url, String clientName, int width, int height, Callback<Bitmap> callback) {
            callback.onResult(mBitmapToFetch);
        }

        @Override
        public void clear() {}

        @Override
        public @ImageFetcherConfig int getConfig() {
            return ImageFetcherConfig.IN_MEMORY_ONLY;
        }

        @Override
        public void destroy() {}
    }

    /** Checks that a text view has a specific maximum number of lines to display. */
    public static TypeSafeMatcher<View> isTextMaxLines(int maxLines) {
        return new TypeSafeMatcher<View>() {
            @Override
            protected boolean matchesSafely(View item) {
                return ((TextView) item).getMaxLines() == maxLines;
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("isTextMaxLines");
            }
        };
    }

    /**
     * Waits until {@code matcher} matches {@code condition}. Will automatically fail after a
     * default timeout.
     */
    public static void waitUntilViewMatchesCondition(
            Matcher<View> matcher, Matcher<View> condition) {
        CriteriaHelper.pollInstrumentationThread(
                new Criteria("Timeout while waiting for " + matcher + " to satisfy " + condition) {
                    @Override
                    public boolean isSatisfied() {
                        try {
                            onView(matcher).check(matches(condition));
                            return true;
                        } catch (NoMatchingViewException e) {
                            // Note: all other exceptions are let through, in particular
                            // AmbiguousViewMatcherException.
                            return false;
                        }
                    }
                });
    }

    /**
     * Creates a {@link BottomSheetController} for the activity, suitable for testing.
     *
     * <p>The returned controller is different from the one returned by {@link
     * ChromeActivity#getBottomSheetController}.
     */
    static BottomSheetController createBottomSheetController(ChromeActivity activity) {
        // Copied from {@link ChromeActivity#initializeBottomSheet}.

        ViewGroup coordinator = activity.findViewById(R.id.coordinator);
        LayoutInflater.from(activity).inflate(R.layout.bottom_sheet, coordinator);
        BottomSheet bottomSheet = coordinator.findViewById(R.id.bottom_sheet);
        bottomSheet.init(coordinator, activity);

        ((BottomContainer) activity.findViewById(R.id.bottom_container))
                .setBottomSheet(bottomSheet);

        return new BottomSheetController(activity, activity.getLifecycleDispatcher(),
                activity.getActivityTabProvider(), activity.getScrim(), bottomSheet,
                activity.getCompositorViewHolder().getLayoutManager().getOverlayPanelManager(),
                /* suppressSheetForContextualSearch= */ false);
    }

    /**
     * Attaches the specified view to the Chrome coordinator. Must be called from the UI thread.
     */
    public static void attachToCoordinator(CustomTabActivity activity, View view) {
        ThreadUtils.assertOnUiThread();
        ViewGroup chromeCoordinatorView =
                activity.findViewById(org.chromium.chrome.autofill_assistant.R.id.coordinator);
        CoordinatorLayout.LayoutParams lp = new CoordinatorLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.gravity = Gravity.BOTTOM;
        chromeCoordinatorView.addView(view, lp);
    }

    /**
     * Starts the CCT test rule on a blank page.
     */
    public static void startOnBlankPage(CustomTabActivityTestRule testRule)
            throws InterruptedException {
        testRule.startCustomTabActivityWithIntent(CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getTargetContext(), "about:blank"));
    }
}
