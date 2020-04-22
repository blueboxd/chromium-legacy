// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.when;

import android.view.Menu;
import android.view.View;
import android.widget.PopupMenu;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkBridge;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for {@link AppMenuPropertiesDelegateImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class AppMenuPropertiesDelegateUnitTest {
    @Mock
    private ActivityTabProvider mActivityTabProvider;
    @Mock
    private Tab mTab;
    @Mock
    private WebContents mWebContents;
    @Mock
    private NavigationController mNavigationController;
    @Mock
    private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabModel mTabModel;
    @Mock
    private TabModel mIncognitoTabModel;
    @Mock
    private ToolbarManager mToolbarManager;
    @Mock
    private View mDecorView;
    @Mock
    private OverviewModeBehavior mOverviewModeBehavior;

    private ObservableSupplierImpl<OverviewModeBehavior> mOverviewModeSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<BookmarkBridge> mBookmarkBridgeSupplier =
            new ObservableSupplierImpl<>();

    private AppMenuPropertiesDelegateImpl mAppMenuPropertiesDelegate;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mOverviewModeSupplier.set(mOverviewModeBehavior);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mToolbarManager.isMenuFromBottom()).thenReturn(false);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn((mTabModel));
        when(mTabModelSelector.getModel(true)).thenReturn((mIncognitoTabModel));
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);

        mAppMenuPropertiesDelegate = Mockito.spy(new AppMenuPropertiesDelegateImpl(
                ContextUtils.getApplicationContext(), mActivityTabProvider,
                mMultiWindowModeStateDispatcher, mTabModelSelector, mToolbarManager, mDecorView,
                mOverviewModeSupplier, mBookmarkBridgeSupplier));
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowPageMenu_Phone() {
        setUpMocksForPageMenu();
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowPageMenu_Tablet() {
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(1);
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowOverviewMenu_Tablet() {
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(true);
        when(mTabModel.getCount()).thenReturn(1);
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(
                MenuGroup.OVERVIEW_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowIconRow_Phone() {
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowIconRow_Tablet() {
        when(mDecorView.getWidth())
                .thenReturn((int) (600
                        * ContextUtils.getApplicationContext()
                                  .getResources()
                                  .getDisplayMetrics()
                                  .density));
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowIconRow_TabletNarrow() {
        when(mDecorView.getWidth())
                .thenReturn((int) (100
                        * ContextUtils.getApplicationContext()
                                  .getResources()
                                  .getDisplayMetrics()
                                  .density));
        Assert.assertTrue(mAppMenuPropertiesDelegate.shouldShowIconRow());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_Ntp() {
        setUpMocksForPageMenu();
        when(mTab.getUrlString()).thenReturn(UrlConstants.NTP_URL);
        when(mTab.isNativePage()).thenReturn(true);

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.request_desktop_site_row_menu_id, R.id.preferences_id, R.id.help_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testPageMenuItems_Phone_RegularPage() {
        setUpMocksForPageMenu();
        when(mTab.getUrlString()).thenReturn("https://google.com");
        when(mTab.isNativePage()).thenReturn(false);
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowPaintPreview(anyBoolean(), any(Tab.class), anyBoolean());
        doReturn(true).when(mAppMenuPropertiesDelegate).shouldShowTranslateMenuItem(any(Tab.class));
        doReturn(R.string.menu_add_to_homescreen)
                .when(mAppMenuPropertiesDelegate)
                .getAddToHomeScreenTitle();

        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.icon_row_menu_id, R.id.new_tab_menu_id,
                R.id.new_incognito_tab_menu_id, R.id.all_bookmarks_menu_id,
                R.id.recent_tabs_menu_id, R.id.open_history_menu_id, R.id.downloads_menu_id,
                R.id.translate_id, R.id.share_row_menu_id, R.id.find_in_page_id,
                R.id.add_to_homescreen_id, R.id.request_desktop_site_row_menu_id,
                R.id.preferences_id, R.id.help_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testOverviewMenuItems_Phone() {
        setUpMocksForOverviewMenu();
        when(mIncognitoTabModel.getCount()).thenReturn(0);

        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(
                MenuGroup.OVERVIEW_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {R.id.new_tab_menu_id, R.id.new_incognito_tab_menu_id,
                R.id.close_all_tabs_menu_id, R.id.preferences_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testOverviewMenuItems_Tablet_NoTabs() {
        setUpIncognitoMocks();
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(0);

        Assert.assertEquals(
                MenuGroup.TABLET_EMPTY_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());

        Menu menu = createTestMenu();
        mAppMenuPropertiesDelegate.prepareMenu(menu, null);

        Integer[] expectedItems = {
                R.id.new_tab_menu_id, R.id.new_incognito_tab_menu_id, R.id.preferences_id};
        assertMenuItemsAreEqual(menu, expectedItems);
    }

    private void setUpMocksForPageMenu() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(false);
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldCheckBookmarkStar(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldEnableDownloadPage(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldShowReaderModePrefs(any(Tab.class));
        setUpIncognitoMocks();
    }

    private void setUpMocksForOverviewMenu() {
        when(mOverviewModeBehavior.overviewVisible()).thenReturn(true);
        when(mTabModelSelector.getTotalTabCount()).thenReturn(1);
        setUpIncognitoMocks();
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mAppMenuPropertiesDelegate).isIncognitoEnabled();
        doReturn(false).when(mAppMenuPropertiesDelegate).isIncognitoManaged();
    }

    private Menu createTestMenu() {
        PopupMenu tempMenu = new PopupMenu(ContextUtils.getApplicationContext(), mDecorView);
        tempMenu.inflate(mAppMenuPropertiesDelegate.getAppMenuLayoutId());
        Menu menu = tempMenu.getMenu();
        return menu;
    }

    private void assertMenuItemsAreEqual(Menu menu, Integer... expectedItems) {
        List<Integer> actualItems = new ArrayList<>();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                actualItems.add(menu.getItem(i).getItemId());
            }
        }

        Assert.assertThat("Populated menu items were:" + getMenuTitles(menu), actualItems,
                Matchers.containsInAnyOrder(expectedItems));
    }

    private String getMenuTitles(Menu menu) {
        StringBuilder items = new StringBuilder();
        for (int i = 0; i < menu.size(); i++) {
            if (menu.getItem(i).isVisible()) {
                items.append("\n").append(menu.getItem(i).getTitle());
            }
        }
        return items.toString();
    }
}
