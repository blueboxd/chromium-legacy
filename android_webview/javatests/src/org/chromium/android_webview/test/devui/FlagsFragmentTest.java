// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThan;

import android.content.Intent;
import android.support.test.rule.ActivityTestRule;
import android.view.View;
import android.widget.EditText;
import android.widget.ListView;

import androidx.test.filters.MediumTest;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.devui.FlagsFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

/**
 * UI tests for {@link FlagsFragment}.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class FlagsFragmentTest {
    @Rule
    public ActivityTestRule mRule =
            new ActivityTestRule<MainActivity>(MainActivity.class, false, false);

    @Before
    public void setUp() throws Exception {
        Intent intent = new Intent();
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_FLAGS);
        mRule.launchActivity(intent);
    }

    private CallbackHelper getFlagUiSearchBarListener() {
        final CallbackHelper helper = new CallbackHelper();
        FlagsFragment.setFilterListener(() -> { helper.notifyCalled(); });
        return helper;
    }

    private static Matcher<View> withHintText(final Matcher<String> stringMatcher) {
        return new TypeSafeMatcher<View>() {
            @Override
            public boolean matchesSafely(View view) {
                if (!(view instanceof EditText)) {
                    return false;
                }
                String hint = ((EditText) view).getHint().toString();
                return stringMatcher.matches(hint);
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("with hint text: ");
                stringMatcher.describeTo(description);
            }
        };
    }

    private static Matcher<View> withHintText(final String expectedHint) {
        return withHintText(is(expectedHint));
    }

    private static Matcher<View> withCount(final Matcher<Integer> intMatcher) {
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

    private static Matcher<View> withCount(final int totalNumFlags) {
        return withCount(is(totalNumFlags));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchEmptyByDefault() throws Throwable {
        onView(withId(R.id.flag_search_bar)).check(matches(withText("")));
        onView(withId(R.id.flag_search_bar)).check(matches(withHintText("Search flags")));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchByName() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSearchByDescription() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("highlight the contents"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.HIGHLIGHT_ALL_WEBVIEWS)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testCaseInsensitive() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("LOGGING"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(allOf(withId(R.id.flag_name), withText(AwSwitches.WEBVIEW_VERBOSE_LOGGING)))
                .check(matches(isDisplayed()));
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMultipleResults() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();

        // This assumes:
        //  * There will always be > 1 flag which mentions WebView explicitly (ex.
        //    HIGHLIGHT_ALL_WEBVIEWS and WEBVIEW_VERBOSE_LOGGING)
        //  * There will always be >= 1 flag which does not mention WebView in its description (ex.
        //    --show-composited-layer-borders).
        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("webview"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(withId(R.id.flags_list))
                .check(matches(withCount(allOf(greaterThan(1), lessThan(totalNumFlags)))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testClearingSearchShowsAllFlags() throws Throwable {
        CallbackHelper helper = getFlagUiSearchBarListener();

        ListView flagsList = mRule.getActivity().findViewById(R.id.flags_list);
        int totalNumFlags = flagsList.getCount();

        int searchBarChangeCount = helper.getCallCount();
        onView(withId(R.id.flag_search_bar)).perform(replaceText("logging"));
        helper.waitForCallback(searchBarChangeCount, 1);
        onView(withId(R.id.flags_list)).check(matches(withCount(1)));

        onView(withId(R.id.flag_search_bar)).perform(replaceText(""));
        helper.waitForCallback(searchBarChangeCount, 2);
        onView(withId(R.id.flags_list)).check(matches(withCount(totalNumFlags)));
    }
}
