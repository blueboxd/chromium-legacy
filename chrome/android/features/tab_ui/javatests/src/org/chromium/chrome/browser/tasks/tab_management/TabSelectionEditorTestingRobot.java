// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.contrib.RecyclerViewActions.actionOnItemAtPosition;
import static android.support.test.espresso.matcher.RootMatchers.withDecorView;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.isEnabled;
import static android.support.test.espresso.matcher.ViewMatchers.withClassName;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import android.os.Build;
import android.support.test.espresso.NoMatchingRootException;
import android.support.test.espresso.Root;
import android.support.test.espresso.matcher.BoundedMatcher;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.hamcrest.Description;
import org.hamcrest.Matcher;
import org.hamcrest.TypeSafeMatcher;
import org.junit.Assert;

/**
 * This is the testing util class for TabSelectionEditor. It's used to perform action and verify
 * result within the TabSelectionEditor.
 */
public class TabSelectionEditorTestingRobot {
    /**
     * @return A root matcher that matches the TabSelectionEditor popup decor view.
     */
    private static Matcher<Root> isTabSelectionEditorPopup() {
        return new TypeSafeMatcher<Root>() {
            @Override
            public void describeTo(Description description) {
                description.appendText("is TabSelectionEditor Popup");
            }

            @Override
            public boolean matchesSafely(Root root) {
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP) {
                    return withDecorView(
                            withClassName(is(TabSelectionEditorLayout.class.getName())))
                            .matches(root);
                } else {
                    return withDecorView(
                            withClassName(is("android.widget.PopupWindow$PopupDecorView")))
                            .matches(root);
                }
            }
        };
    }

    /**
     * @return A view matcher that matches the item is selected.
     */
    private static Matcher<View> itemIsSelected() {
        return new BoundedMatcher<View, SelectableTabGridView>(SelectableTabGridView.class) {
            private SelectableTabGridView mSelectableTabGridView;
            @Override
            protected boolean matchesSafely(SelectableTabGridView selectableTabGridView) {
                mSelectableTabGridView = selectableTabGridView;

                return mSelectableTabGridView.isSelected() && actionButtonSelected()
                        && highlightIndicatorIsVisible();
            }

            @Override
            public void describeTo(Description description) {
                description.appendText("Item is selected");
            }

            private boolean actionButtonSelected() {
                return mSelectableTabGridView.getResources().getInteger(
                               org.chromium.chrome.tab_ui.R.integer.list_item_level_selected)
                        == mSelectableTabGridView
                                   .findViewById(org.chromium.chrome.tab_ui.R.id.action_button)
                                   .getBackground()
                                   .getLevel();
            }

            private boolean highlightIndicatorIsVisible() {
                if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                    return mSelectableTabGridView
                                   .findViewById(org.chromium.chrome.tab_ui.R.id
                                                         .selected_view_below_lollipop)
                                   .getVisibility()
                            == View.VISIBLE;
                } else {
                    return mSelectableTabGridView.getForeground() != null;
                }
            }
        };
    }

    public final TabSelectionEditorTestingRobot.Result resultRobot;
    public final TabSelectionEditorTestingRobot.Action actionRobot;

    TabSelectionEditorTestingRobot() {
        resultRobot = new TabSelectionEditorTestingRobot.Result();
        actionRobot = new TabSelectionEditorTestingRobot.Action();
    }

    /**
     * This Robot is used to perform action within the TabSelectionEditor.
     */
    class Action {
        TabSelectionEditorTestingRobot.Action clickItemAtAdapterPosition(int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(actionOnItemAtPosition(position, click()));
            return this;
        }

        TabSelectionEditorTestingRobot.Action clickToolbarActionButton() {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(click());
            return this;
        }

        TabSelectionEditorTestingRobot.Action clickToolbarNavigationButton() {
            onView(allOf(withContentDescription(org.chromium.chrome.tab_ui.R.string.close),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .perform(click());
            return this;
        }
    }

    /**
     * This Robot is used to verify result within the TabSelectionEditor.
     */
    class Result {
        TabSelectionEditorTestingRobot.Result verifyTabSelectionEditorIsVisible() {
            onView(withId(org.chromium.chrome.tab_ui.R.id.selectable_list))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyTabSelectionEditorIsHidden() {
            try {
                onView(withId(org.chromium.chrome.tab_ui.R.id.selectable_list))
                        .inRoot(isTabSelectionEditorPopup())
                        .check(matches(isDisplayed()));
            } catch (NoMatchingRootException e) {
                return this;
            }

            assert false : "TabSelectionEditor should be hidden, but it's not.";
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyToolbarSelectionTextWithResourceId(
                int resourceId) {
            onView(withText(resourceId))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyToolbarSelectionText(String text) {
            onView(withText(text))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isDisplayed()));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonWithResourceId(
                int resourceId) {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(withText(resourceId)));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonWithText(String text) {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(withText(text)));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonDisabled() {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(not(isEnabled())));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyToolbarActionButtonEnabled() {
            onView(allOf(withId(org.chromium.chrome.tab_ui.R.id.action_button),
                           withParent(withId(org.chromium.chrome.tab_ui.R.id.action_bar))))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(isEnabled()));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyHasAtLeastNItemVisible(int count) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check((v, noMatchException) -> {
                        if (noMatchException != null) throw noMatchException;

                        Assert.assertTrue(v instanceof RecyclerView);
                        Assert.assertTrue(((RecyclerView) v).getChildCount() >= count);
                    });
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyAdapterHasItemCount(int count) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(RecyclerViewMatcherUtils.adapterHasItemCount(count)));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyItemNotSelectedAtAdapterPosition(int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(
                            not(RecyclerViewMatcherUtils.atPosition(position, itemIsSelected()))));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyItemSelectedAtAdapterPosition(int position) {
            onView(withId(org.chromium.chrome.tab_ui.R.id.tab_list_view))
                    .inRoot(isTabSelectionEditorPopup())
                    .check(matches(
                            RecyclerViewMatcherUtils.atPosition(position, itemIsSelected())));
            return this;
        }

        TabSelectionEditorTestingRobot.Result verifyUndoSnackbarWithTextIsShown(String text) {
            onView(withText(text)).check(matches(isDisplayed()));
            return this;
        }
    }
}
