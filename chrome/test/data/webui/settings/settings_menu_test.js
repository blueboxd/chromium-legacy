// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
// #import {pageVisibility, Router, routes} from 'chrome://settings/settings.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {isChromeOS} from 'chrome://resources/js/cr.m.js';
// clang-format on

suite('SettingsMenu', function() {
  let settingsMenu = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: true,
    });
  });

  setup(function() {
    PolymerTest.clearBody();
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = settings.pageVisibility;
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('advancedOpenedBinding', function() {
    assertFalse(settingsMenu.advancedOpened);
    settingsMenu.advancedOpened = true;
    Polymer.dom.flush();
    assertTrue(settingsMenu.$.advancedSubmenu.opened);

    settingsMenu.advancedOpened = false;
    Polymer.dom.flush();
    assertFalse(settingsMenu.$.advancedSubmenu.opened);
  });

  test('tapAdvanced', function() {
    assertFalse(settingsMenu.advancedOpened);

    const advancedToggle = settingsMenu.$$('#advancedButton');
    assertTrue(!!advancedToggle);

    advancedToggle.click();
    Polymer.dom.flush();
    assertTrue(settingsMenu.$.advancedSubmenu.opened);

    advancedToggle.click();
    Polymer.dom.flush();
    assertFalse(settingsMenu.$.advancedSubmenu.opened);
  });

  test('upAndDownIcons', function() {
    // There should be different icons for a top level menu being open
    // vs. being closed. E.g. arrow-drop-up and arrow-drop-down.
    const ironIconElement = settingsMenu.$$('#advancedButton iron-icon');
    assertTrue(!!ironIconElement);

    settingsMenu.advancedOpened = true;
    Polymer.dom.flush();
    const openIcon = ironIconElement.icon;
    assertTrue(!!openIcon);

    settingsMenu.advancedOpened = false;
    Polymer.dom.flush();
    assertNotEquals(openIcon, ironIconElement.icon);
  });

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', function() {
    // As of iron-selector 2.x, need to force iron-selector to update before
    // clicking items on it, or wait for 'iron-items-changed'
    const ironSelector = settingsMenu.$$('iron-selector');
    ironSelector.forceSynchronousItemUpdate();

    const urlParams = new URLSearchParams('search=foo');
    settings.Router.getInstance().navigateTo(settings.routes.BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        settings.Router.getInstance().getQueryParameters().toString());
    settingsMenu.$.people.click();
    assertEquals(
        '', settings.Router.getInstance().getQueryParameters().toString());
  });
});

suite('SettingsMenuReset', function() {
  let settingsMenu = null;

  setup(function() {
    PolymerTest.clearBody();
    settings.Router.getInstance().navigateTo(settings.routes.RESET, '');
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('openResetSection', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);
  });

  test('navigateToAnotherSection', function() {
    const selector = settingsMenu.$.subMenu;
    let path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);

    settings.Router.getInstance().navigateTo(settings.routes.PEOPLE, '');
    Polymer.dom.flush();

    path = new window.URL(selector.selected).pathname;
    assertEquals('/people', path);
  });

  test('navigateToBasic', function() {
    const selector = settingsMenu.$.subMenu;
    const path = new window.URL(selector.selected).pathname;
    assertEquals('/reset', path);

    settings.Router.getInstance().navigateTo(settings.routes.BASIC, '');
    Polymer.dom.flush();

    // BASIC has no sub page selected.
    assertFalse(!!selector.selected);
  });

  test('pageVisibility', function() {
    function assertPageVisibility(expectedHidden) {
      assertEquals(expectedHidden, settingsMenu.$$('#people').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#appearance').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#onStartup').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#advancedButton').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#advancedSubmenu').hidden);
      assertEquals(expectedHidden, settingsMenu.$$('#reset').hidden);

      if (!cr.isChromeOS) {
        assertEquals(expectedHidden, settingsMenu.$$('#defaultBrowser').hidden);
      }
    }

    // The default pageVisibility should not cause menu items to be hidden.
    assertPageVisibility(false);

    // Set the visibility of the pages under test to "false".
    settingsMenu.pageVisibility = Object.assign(settings.pageVisibility || {}, {
      advancedSettings: false,
      appearance: false,
      defaultBrowser: false,
      multidevice: false,
      onStartup: false,
      people: false,
      reset: false
    });
    Polymer.dom.flush();

    // Now, the menu items should be hidden.
    assertPageVisibility(true);
  });

  test('safetyCheckInMenu', function() {
    Polymer.dom.flush();
    assertTrue(!!settingsMenu.$$('#safetyCheck'));
  });
});

suite('SettingsMenuPrivacyRedesignFlagOff', function() {
  let settingsMenu = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
  });

  setup(function() {
    PolymerTest.clearBody();
    settingsMenu = document.createElement('settings-menu');
    settingsMenu.pageVisibility = settings.pageVisibility;
    document.body.appendChild(settingsMenu);
  });

  teardown(function() {
    settingsMenu.remove();
  });

  test('safetyCheckNotInMenu', function() {
    Polymer.dom.flush();
    assertFalse(!!settingsMenu.$$('#safetyCheck'));
  });
});
