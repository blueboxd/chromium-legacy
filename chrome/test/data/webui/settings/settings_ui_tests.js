// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://settings/settings.js';
import {eventToPromise} from 'chrome://test/test_util.m.js';
// clang-format on

/** @fileoverview Suite of tests for the Settings layout. */
suite('settings-ui', function() {
  let toolbar;
  let ui;

  setup(function() {
    PolymerTest.clearBody();
    ui = document.createElement('settings-ui');
    document.body.appendChild(ui);
    return CrSettingsPrefs.initialized.then(() => {
      flush();
    });
  });

  test('showing menu in toolbar is dependent on narrow mode', function() {
    toolbar = ui.$$('cr-toolbar');
    assertTrue(!!toolbar);
    toolbar.narrow = true;
    assertTrue(toolbar.showMenu);

    toolbar.narrow = false;
    assertFalse(toolbar.showMenu);
  });

  test('app drawer', function() {
    assertEquals(null, ui.$$('cr-drawer settings-menu'));
    const drawer = ui.$.drawer;
    assertFalse(!!drawer.open);

    const whenDone = eventToPromise('cr-drawer-opened', drawer);
    drawer.openDrawer();
    flush();

    // Validate that dialog is open and menu is shown so it will animate.
    assertTrue(drawer.open);
    assertTrue(!!ui.$$('cr-drawer settings-menu'));

    return whenDone
        .then(function() {
          const whenClosed = eventToPromise('close', drawer);
          drawer.cancel();
          return whenClosed;
        })
        .then(() => {
          // Drawer is closed, but menu is still stamped so
          // its contents remain visible as the drawer slides
          // out.
          assertTrue(!!ui.$$('cr-drawer settings-menu'));
        });
  });

  // TODO(rbpotter): Fix or delete this test. It is flaky (times out ~1 in 10
  // runs) locally on a Linux non-optimized build.
  test.skip('app drawer closes when exiting narrow mode', async () => {
    const drawer = ui.$.drawer;
    const toolbar = ui.$$('cr-toolbar');

    // Mimic narrow mode and open the drawer
    toolbar.narrow = true;
    drawer.openDrawer();
    flush();
    await eventToPromise('cr-drawer-opened', drawer);

    toolbar.narrow = false;
    flush();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
  });

  test('advanced UIs stay in sync', function() {
    const main = ui.$$('settings-main');
    const floatingMenu = ui.$$('#left settings-menu');
    assertTrue(!!main);
    assertTrue(!!floatingMenu);

    assertFalse(!!ui.$$('cr-drawer settings-menu'));
    assertFalse(ui.advancedOpenedInMain_);
    assertFalse(ui.advancedOpenedInMenu_);
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(main.advancedToggleExpanded);

    main.advancedToggleExpanded = true;
    flush();

    assertFalse(!!ui.$$('cr-drawer settings-menu'));
    assertTrue(ui.advancedOpenedInMain_);
    assertTrue(ui.advancedOpenedInMenu_);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(main.advancedToggleExpanded);

    ui.$.drawerTemplate.if = true;
    flush();

    const drawerMenu = ui.$$('cr-drawer settings-menu');
    assertTrue(!!drawerMenu);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(drawerMenu.advancedOpened);

    // Collapse 'Advanced' in the menu
    drawerMenu.$.advancedButton.click();
    flush();

    // Collapsing it in the menu should not collapse it in the main area
    assertFalse(drawerMenu.advancedOpened);
    assertFalse(floatingMenu.advancedOpened);
    assertFalse(ui.advancedOpenedInMenu_);
    assertTrue(main.advancedToggleExpanded);
    assertTrue(ui.advancedOpenedInMain_);

    // Expand both 'Advanced's again
    drawerMenu.$.advancedButton.click();

    // Collapse 'Advanced' in the main area
    main.advancedToggleExpanded = false;
    flush();

    // Collapsing it in the main area should not collapse it in the menu
    assertFalse(ui.advancedOpenedInMain_);
    assertTrue(drawerMenu.advancedOpened);
    assertTrue(floatingMenu.advancedOpened);
    assertTrue(ui.advancedOpenedInMenu_);
  });

  test('URL initiated search propagates to search box', function() {
    toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
    const searchField =
        /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());
    assertEquals('', searchField.getSearchInput().value);

    const query = 'foo';
    Router.getInstance().navigateTo(
        routes.BASIC, new URLSearchParams(`search=${query}`));
    assertEquals(query, searchField.getSearchInput().value);
  });

  test('search box initiated search propagates to URL', function() {
    toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
    const searchField =
        /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());

    Router.getInstance().navigateTo(
        routes.BASIC, /* dynamicParams */ null,
        /* removeSearch */ true);
    assertEquals('', searchField.getSearchInput().value);
    assertFalse(Router.getInstance().getQueryParameters().has('search'));

    let value = 'GOOG';
    searchField.setValue(value);
    assertEquals(
        value, Router.getInstance().getQueryParameters().get('search'));

    // Test that search queries are properly URL encoded.
    value = '+++';
    searchField.setValue(value);
    assertEquals(
        value, Router.getInstance().getQueryParameters().get('search'));
  });

  test('whitespace only search query is ignored', function() {
    toolbar = /** @type {!CrToolbarElement} */ (ui.$$('cr-toolbar'));
    const searchField =
        /** @type {CrToolbarSearchFieldElement} */ (toolbar.getSearchField());
    searchField.setValue('    ');
    let urlParams = Router.getInstance().getQueryParameters();
    assertFalse(urlParams.has('search'));

    searchField.setValue('   foo');
    urlParams = Router.getInstance().getQueryParameters();
    assertEquals('foo', urlParams.get('search'));

    searchField.setValue('   foo ');
    urlParams = Router.getInstance().getQueryParameters();
    assertEquals('foo ', urlParams.get('search'));

    searchField.setValue('   ');
    urlParams = Router.getInstance().getQueryParameters();
    assertFalse(urlParams.has('search'));
  });
});
