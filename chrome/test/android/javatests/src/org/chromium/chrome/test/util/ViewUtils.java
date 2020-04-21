// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.support.test.espresso.Espresso;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.ViewAssertion;
import android.support.test.espresso.ViewInteraction;
import android.support.test.espresso.matcher.ViewMatchers;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import org.hamcrest.Matcher;

import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;

import java.lang.annotation.Retention;
import java.util.ArrayList;
import java.util.List;

/**
 * Collection of utilities helping to clarify expectations on views in tests.
 */
public class ViewUtils {
    @Retention(SOURCE)
    @IntDef(flag = true, value = {VIEW_VISIBLE, VIEW_INVISIBLE, VIEW_GONE, VIEW_NULL})
    public @interface ExpectedViewState {}
    public static final int VIEW_VISIBLE = 1;
    public static final int VIEW_INVISIBLE = 1 << 1;
    public static final int VIEW_GONE = 1 << 2;
    public static final int VIEW_NULL = 1 << 3;

    private static class ExpectedViewCriteria extends Criteria {
        private final Matcher<View> mViewMatcher;
        private final @ExpectedViewState int mViewState;
        private final ViewGroup mRootView;

        ExpectedViewCriteria(
                Matcher<View> viewMatcher, @ExpectedViewState int viewState, ViewGroup rootView) {
            mViewMatcher = viewMatcher;
            mViewState = viewState;
            mRootView = rootView;
        }

        @Override
        public boolean isSatisfied() {
            List<View> matchedViews = new ArrayList<>();
            findMatchingChildren(mRootView, matchedViews);
            if (matchedViews.size() > 1) {
                updateFailureReason("Multiple views matched: " + mViewMatcher.toString());
                return false;
            }
            return matchedViews.size() == 0 ? hasViewExpectedState(null)
                                            : hasViewExpectedState(matchedViews.get(0));
        }

        private void findMatchingChildren(ViewGroup root, List<View> matchedViews) {
            for (int i = 0; i < root.getChildCount(); i++) {
                View view = root.getChildAt(i);
                if (mViewMatcher.matches(view)) matchedViews.add(view);
                if (view instanceof ViewGroup) {
                    findMatchingChildren((ViewGroup) view, matchedViews);
                }
            };
        }

        private boolean hasViewExpectedState(View view) {
            if (view == null) {
                updateFailureReason("No view found to match: " + mViewMatcher.toString());
                return (mViewState & VIEW_NULL) != 0;
            }
            switch (view.getVisibility()) {
                case View.VISIBLE:
                    updateFailureReason("View matching '" + mViewMatcher.toString()
                            + "' is unexpectedly visible!");
                    return (mViewState & VIEW_VISIBLE) != 0;
                case View.INVISIBLE:
                    updateFailureReason("View matching '" + mViewMatcher.toString()
                            + "' is unexpectedly invisible!");
                    return (mViewState & VIEW_INVISIBLE) != 0;
                case View.GONE:
                    updateFailureReason("View matching '" + mViewMatcher.toString()
                            + "' is unexpectedly gone!");
                    return (mViewState & VIEW_GONE) != 0;
            }
            assert false; // Not Reached.
            return false;
        }
    }

    private ViewUtils() {}

    /**
     * Waits until a view matching the given matches any of the given {@link ExpectedViewState}s.
     * Fails if the matcher applies to multiple views. Times out if no view was found while waiting
     * up to {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    public static void waitForView(
            ViewGroup root, Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        CriteriaHelper.pollUiThread(new ExpectedViewCriteria(viewMatcher, viewState, root));
    }

    /**
     * Waits until a view matching the given matches any of the given {@link ExpectedViewState}s.
     * Fails if the matcher applies to multiple views. Times out if no view was found while waiting
     * up to {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * This should be used on {@link ViewInteraction#check} with a {@link ViewGroup}. For example,
     * the following usage assumes the root view is a {@link ViewGroup}.
     * <pre>
     *   onView(isRoot()).check(waitForView(withId(R.id.example_id), VIEW_GONE));
     * </pre>
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @param viewState State that the matching view should be in. If multiple states are passed,
     *                  the waiting will stop if at least one applies.
     */
    public static ViewAssertion waitForView(
            Matcher<View> viewMatcher, @ExpectedViewState int viewState) {
        return (View view, NoMatchingViewException noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            CriteriaHelper.pollUiThread(
                    new ExpectedViewCriteria(viewMatcher, viewState, (ViewGroup) view));
        };
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views.  Times out if no view was found while waiting up to
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param root The view group to search in.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static void waitForView(ViewGroup root, Matcher<View> viewMatcher) {
        waitForView(root, viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views.  Times out if no view was found while waiting up to
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param viewMatcher The matcher matching the view that should be waited for.
     */
    public static ViewAssertion waitForView(Matcher<View> viewMatcher) {
        return waitForView(viewMatcher, VIEW_VISIBLE);
    }

    /**
     * Waits until a visible view matching the given matcher appears. Fails if the matcher applies
     * to multiple views.  Times out if no view was found while waiting up to
     * {@link CriteriaHelper#DEFAULT_MAX_TIME_TO_POLL} milliseconds.
     * @param viewMatcher The matcher matching the view that should be waited for.
     * @return An interaction on the matching view.
     */
    public static ViewInteraction onViewWaiting(Matcher<View> viewMatcher) {
        Espresso.onView(ViewMatchers.isRoot())
                .check((View view, NoMatchingViewException noMatchException) -> {
                    if (noMatchException != null) throw noMatchException;
                    CriteriaHelper.pollUiThread(
                            new ExpectedViewCriteria(viewMatcher, VIEW_VISIBLE, (ViewGroup) view));
                });
        return Espresso.onView(viewMatcher);
    }

    /**
     * Wait until the specified view has finished layout updates.
     * @param view The specified view.
     */
    public static void waitForStableView(final View view) {
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (view.isDirty()) {
                    updateFailureReason("The view is dirty.");
                    return false;
                }

                if (view.isLayoutRequested()) {
                    updateFailureReason("The view has layout requested.");
                    return false;
                }

                return true;
            }
        });
    }
}
