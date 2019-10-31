// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.support.test.filters.MediumTest;
import android.support.test.rule.UiThreadTestRule;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.SubMenu;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ui.DummyUiActivityTestCase;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Tests for {@link AppMenuAdapter}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AppMenuAdapterTest extends DummyUiActivityTestCase {
    private static class TestClickHandler implements AppMenuAdapter.OnClickHandler {
        public CallbackHelper onClickCallback = new CallbackHelper();
        public MenuItem lastClickedItem;

        public CallbackHelper onLongClickCallback = new CallbackHelper();
        public MenuItem lastLongClickedItem;

        @Override
        public void onItemClick(MenuItem menuItem) {
            onClickCallback.notifyCalled();
            lastClickedItem = menuItem;
        }

        @Override
        public boolean onItemLongClick(MenuItem menuItem, View view) {
            onLongClickCallback.notifyCalled();
            lastLongClickedItem = menuItem;
            return true;
        }
    }

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    private static final String TITLE_1 = "Menu Item One";
    private static final String TITLE_2 = "Menu Item Two";
    private static final String TITLE_3 = "Menu Item Three";
    private static final String TITLE_4 = "Menu Item Four";
    private static final String TITLE_5 = "Menu Item Five";

    private TestClickHandler mClickHandler;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();

        MockitoAnnotations.initMocks(this);
        mClickHandler = new TestClickHandler();
    }

    @Test
    @MediumTest
    public void testStandardMenuItem() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(0, TITLE_1));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);
        Assert.assertEquals("Wrong item view type", AppMenuAdapter.MenuItemType.STANDARD,
                adapter.getItemViewType(0));

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView1 = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView1.getText());
    }

    @Test
    @MediumTest
    public void testConvertView_Reused_StandardMenuItem() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(0, TITLE_1));
        items.add(buildMenuItem(1, TITLE_2));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertEquals("Convert view should have been re-used.", view1, view2);
        Assert.assertEquals("Title should have been updated", TITLE_2, titleView.getText());
    }

    @Test
    @MediumTest
    public void testConvertView_NotReused() {
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(0, TITLE_1));
        items.add(buildTitleMenuItem(1, 2, TITLE_2, 3, TITLE_3));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, null);

        Assert.assertEquals("Wrong item view type for item 1", AppMenuAdapter.MenuItemType.STANDARD,
                adapter.getItemViewType(0));
        Assert.assertEquals("Wrong item view type for item 2",
                AppMenuAdapter.MenuItemType.TITLE_BUTTON, adapter.getItemViewType(1));

        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        TextView titleView = view1.findViewById(R.id.menu_item_text);

        Assert.assertEquals("Incorrect title text for item 1", TITLE_1, titleView.getText());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertNotEquals("Convert view should not have been re-used.", view1, view2);
        Assert.assertEquals(
                "Title for view 1 should have not have been updated", TITLE_1, titleView.getText());
    }

    @Test
    @MediumTest
    public void testCustomViewBinders() {
        // Set-up custom binders.
        CustomViewBinderOne customBinder1 = new CustomViewBinderOne();
        CustomViewBinderTwo customBinder2 = new CustomViewBinderTwo();
        List<CustomViewBinder> customViewBinders = new ArrayList<>();
        customViewBinders.add(customBinder1);
        customViewBinders.add(customBinder2);

        Assert.assertEquals(3, AppMenuAdapter.getCustomViewTypeCount(customViewBinders));

        // Set-up menu items.
        List<MenuItem> items = new ArrayList<>();
        items.add(buildMenuItem(0, TITLE_1));
        items.add(buildMenuItem(customBinder1.supportedId1, TITLE_2));
        items.add(buildMenuItem(customBinder1.supportedId2, TITLE_3));
        items.add(buildMenuItem(customBinder1.supportedId3, TITLE_4));
        items.add(buildMenuItem(customBinder2.supportedId1, TITLE_5));

        AppMenuAdapter adapter = new AppMenuAdapter(
                mClickHandler, items, getActivity().getLayoutInflater(), 0, customViewBinders);
        Map<CustomViewBinder, Integer> offsetMap = adapter.getViewTypeOffsetMapForTests();

        Assert.assertEquals("Incorrect view type offset for binder 1",
                AppMenuAdapter.MenuItemType.NUM_ENTRIES, (int) offsetMap.get(customBinder1));
        Assert.assertEquals("Incorrect view type offset for binder 2",
                AppMenuAdapter.MenuItemType.NUM_ENTRIES + CustomViewBinderOne.VIEW_TYPE_COUNT,
                (int) offsetMap.get(customBinder2));

        // Check that the item view types are correct.
        Assert.assertEquals("Wrong item view type for item 1", AppMenuAdapter.MenuItemType.STANDARD,
                adapter.getItemViewType(0));
        Assert.assertEquals("Wrong item view type for item 2",
                CustomViewBinderOne.VIEW_TYPE_1 + AppMenuAdapter.MenuItemType.NUM_ENTRIES,
                adapter.getItemViewType(1));
        Assert.assertEquals("Wrong item view type for item 3",
                CustomViewBinderOne.VIEW_TYPE_1 + AppMenuAdapter.MenuItemType.NUM_ENTRIES,
                adapter.getItemViewType(2));
        Assert.assertEquals("Wrong item view type for item 4",
                CustomViewBinderOne.VIEW_TYPE_2 + AppMenuAdapter.MenuItemType.NUM_ENTRIES,
                adapter.getItemViewType(3));
        Assert.assertEquals("Wrong item view type for item 5",
                CustomViewBinderTwo.VIEW_TYPE_1 + AppMenuAdapter.MenuItemType.NUM_ENTRIES
                        + CustomViewBinderOne.VIEW_TYPE_COUNT,
                adapter.getItemViewType(4));

        // Check that custom binders and convert views are used as expected.
        ViewGroup parentView = getActivity().findViewById(android.R.id.content);
        View view1 = adapter.getView(0, null, parentView);
        Assert.assertEquals("Binder1 incorrectly called", 0,
                customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());

        View view2 = adapter.getView(1, view1, parentView);
        Assert.assertEquals(
                "Binder1 not called", 1, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertNotEquals("Convert view should not have been re-used", view1, view2);

        View view3 = adapter.getView(2, view2, parentView);
        Assert.assertEquals(
                "Binder1 not called", 2, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Convert view should have been re-used", view2, view3);

        View view4 = adapter.getView(3, view2, parentView);
        Assert.assertEquals(
                "Binder1 not called", 3, customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals("Binder2 incorrectly called", 0,
                customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertNotEquals("Convert view should not have been re-used", view2, view4);

        View view5 = adapter.getView(4, view2, parentView);
        Assert.assertEquals("Binder1 incorrectly called", 3,
                customBinder1.getViewItemCallbackHelper.getCallCount());
        Assert.assertEquals(
                "Binder2 not called", 1, customBinder2.getViewItemCallbackHelper.getCallCount());
        Assert.assertNotEquals("Convert view should not have been re-used", view2, view5);
    }

    private static MenuItem buildMenuItem(int id, CharSequence title) {
        MenuItem item = Mockito.mock(MenuItem.class);
        Mockito.when(item.getItemId()).thenReturn(id);
        Mockito.when(item.getTitle()).thenReturn(title);
        return item;
    }

    private static MenuItem buildTitleMenuItem(
            int id, int subId1, CharSequence title1, int subId2, CharSequence title2) {
        MenuItem item = Mockito.mock(MenuItem.class);
        SubMenu subMenu = Mockito.mock(SubMenu.class);
        Mockito.when(item.getItemId()).thenReturn(id);
        Mockito.when(item.hasSubMenu()).thenReturn(true);
        Mockito.when(item.getSubMenu()).thenReturn(subMenu);

        Mockito.when(subMenu.size()).thenReturn(2);
        MenuItem subMenuItem1 = buildMenuItem(subId1, title1);
        MenuItem subMenuItem2 = buildMenuItem(subId2, title2);

        Mockito.when(subMenu.getItem(0)).thenReturn(subMenuItem1);
        Mockito.when(subMenu.getItem(1)).thenReturn(subMenuItem2);

        return item;
    }

    private static class CustomViewBinderOne implements CustomViewBinder {
        public static final int VIEW_TYPE_1 = 0;
        public static final int VIEW_TYPE_2 = 1;
        public static final int VIEW_TYPE_COUNT = 2;

        public int supportedId1;
        public int supportedId2;
        public int supportedId3;

        public CallbackHelper getViewItemCallbackHelper = new CallbackHelper();

        public CustomViewBinderOne() {
            supportedId1 = View.generateViewId();
            supportedId2 = View.generateViewId();
            supportedId3 = View.generateViewId();
        }

        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        @Override
        public int getItemViewType(int id) {
            if (id == supportedId1 || id == supportedId2) {
                return VIEW_TYPE_1;
            } else if (id == supportedId3) {
                return VIEW_TYPE_2;
            } else {
                return NOT_HANDLED;
            }
        }

        @Override
        public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
                LayoutInflater inflater) {
            int itemId = item.getItemId();
            Assert.assertTrue("getView called for incorrect item",
                    itemId == supportedId1 || itemId == supportedId2 || itemId == supportedId3);

            getViewItemCallbackHelper.notifyCalled();

            return convertView != null ? convertView : new View(parent.getContext());
        }

        @Override
        public boolean supportsEnterAnimation(int id) {
            return true;
        }
    }

    private static class CustomViewBinderTwo implements CustomViewBinder {
        public static final int VIEW_TYPE_1 = 0;
        public static final int VIEW_TYPE_COUNT = 1;

        public int supportedId1;
        public CallbackHelper getViewItemCallbackHelper = new CallbackHelper();

        public CustomViewBinderTwo() {
            supportedId1 = View.generateViewId();
        }

        @Override
        public int getViewTypeCount() {
            return VIEW_TYPE_COUNT;
        }

        @Override
        public int getItemViewType(int id) {
            return id == supportedId1 ? VIEW_TYPE_1 : NOT_HANDLED;
        }

        @Override
        public View getView(MenuItem item, @Nullable View convertView, ViewGroup parent,
                LayoutInflater inflater) {
            int itemId = item.getItemId();
            Assert.assertTrue("getView called for incorrect item", itemId == supportedId1);

            getViewItemCallbackHelper.notifyCalled();

            return convertView != null ? convertView : new View(parent.getContext());
        }

        @Override
        public boolean supportsEnterAnimation(int id) {
            return true;
        }
    }
}
