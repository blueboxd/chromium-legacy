// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static org.hamcrest.Matchers.is;

import android.content.ClipboardManager;
import android.content.Context;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;

import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;

/**
 * Util methods for developer UI tests.
 */
public class DeveloperUiTestUtils {
    /**
     * Matches that a {@link ListView} has a specific number of items.
     *
     * @param intMatcher {@line Matcher} class that matches a given integer.
     */
    public static Matcher<View> withCount(final Matcher<Integer> intMatcher) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof ListView)) {
                    return false;
                }
                int count = ((ListView) view).getCount();
                return intMatcher.matches(count);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with child-count: ");
                intMatcher.describeTo(description);
            }
        };
    }

    /**
     * Matches that a {@link ListView} has a specific number of items
     */
    public static Matcher<View> withCount(final int itemCount) {
        return withCount(is(itemCount));
    }

    /**
     * Matches a view that has this layout
     * android_webview/nonembedded/java/res_devui/layout/two_line_list_item.xml and has the given
     * title.
     */
    public static Matcher<View> withTitle(final Matcher<String> stringMatcher) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof LinearLayout)) {
                    return false;
                }
                TextView textView = (TextView) view.findViewById(android.R.id.text1);
                // Make sure only the immediate parent View is matched.
                return textView != null && textView.getParent() == view
                        && stringMatcher.matches(textView.getText());
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with title: ");
                stringMatcher.describeTo(description);
            }
        };
    }

    /**
     * Matches a view that has this layout
     * android_webview/nonembedded/java/res_devui/layout/two_line_list_item.xml and has the given
     * title.
     */
    public static Matcher<View> withTitle(String title) {
        return withTitle(is(title));
    }

    /**
     * Matches a view that has this layout
     * android_webview/nonembedded/java/res_devui/layout/two_line_list_item.xml and has the given
     * subtitle.
     */
    public static Matcher<View> withSubtitle(final Matcher<String> stringMatcher) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof LinearLayout)) {
                    return false;
                }
                TextView textView = (TextView) view.findViewById(android.R.id.text2);
                // Make sure only the immediate parent View is matched.
                return textView != null && textView.getParent() == view
                        && stringMatcher.matches(textView.getText());
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with subtitle: ");
                stringMatcher.describeTo(description);
            }
        };
    }

    /**
     * Matches a view that has this layout
     * android_webview/nonembedded/java/res_devui/layout/two_line_list_item.xml and has the given
     * subtitle.
     */
    public static Matcher<View> withSubtitle(String subtitle) {
        return withSubtitle(is(subtitle));
    }

    public static String getClipBoardTextOnUiThread(Context context) throws ExecutionException {
        // ClipManager service has to be called on the UI main thread.
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            ClipboardManager clipboardManager =
                    (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
            return clipboardManager.getPrimaryClip().getItemAt(0).getText().toString();
        });
    }

    // Don't instantiate this class.
    private DeveloperUiTestUtils() {}
}
