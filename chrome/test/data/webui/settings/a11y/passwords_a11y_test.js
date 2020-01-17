// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Define accessibility tests for the PASSWORDS route. */

// SettingsAccessibilityTest fixture.
GEN_INCLUDE([
  'settings_accessibility_test.js',
]);

/**
 * Test fixture for PASSWORDS
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsA11yPasswords() {}

SettingsA11yPasswords.prototype = {
  __proto__: SettingsAccessibilityTest.prototype,

  // Include files that define the mocha tests.
  extraLibraries: SettingsAccessibilityTest.prototype.extraLibraries.concat([
    '../../test_browser_proxy.js',
    '../passwords_and_autofill_fake_data.js',
    '../test_password_manager_proxy.js',
  ]),
};

AccessibilityTest.define('SettingsA11yPasswords', {
  /** @override */
  name: 'PASSWORDS',
  /** @type {PasswordManager} */
  passwordManager: null,
  /** @type {PasswordsSectionElement}*/
  passwordsSection: null,
  // TODO(hcarmona): Create function that overrides defaults to simplify this.
  axeOptions: Object.assign({}, SettingsAccessibilityTest.axeOptions, {
    'rules': Object.assign({}, SettingsAccessibilityTest.axeOptions.rules, {
      // TODO(hcarmona): Investigate flakyness and enable these tests.
      // Disable rules flaky for CFI build.
      'meta-viewport': {enabled: false},
      'list': {enabled: false},
      'frame-title': {enabled: false},
      'label': {enabled: false},
      'hidden-content': {enabled: false},
      'aria-valid-attr-value': {enabled: false},
      'button-name': {enabled: false},
    }),
  }),
  /** @override */
  setup: function() {
    return new Promise((resolve) => {
      // Reset to a blank page.
      PolymerTest.clearBody();

      // Set the URL to be that of specific route to load upon injecting
      // settings-ui. Simply calling
      // settings.Router.getInstance().navigateTo(route) prevents use of mock
      // APIs for fake data.
      window.history.pushState(
          'object or string', 'Test', settings.routes.PASSWORDS.path);

      PasswordManagerImpl.instance_ = new TestPasswordManagerProxy();
      this.passwordManager = PasswordManagerImpl.getInstance();

      const settingsUi = document.createElement('settings-ui');

      // The settings section will expand to load the PASSWORDS route (based on
      // the URL set above) once the settings-ui element is attached
      settingsUi.addEventListener('settings-section-expanded', () => {
        // Passwords section should be loaded before setup is complete.
        this.passwordsSection = settingsUi.$$('settings-main')
                                    .$$('settings-basic-page')
                                    .$$('settings-autofill-page')
                                    .$$('passwords-section');
        assertTrue(!!this.passwordsSection);

        assertEquals(
            this.passwordManager, this.passwordsSection.passwordManager_);

        resolve();
      });

      document.body.appendChild(settingsUi);
    });
  },

  /** @override */
  tests: {
    'Accessible with 0 passwords': function() {
      assertEquals(0, this.passwordsSection.savedPasswords.length);
    },
    'Accessible with 10 passwords': function() {
      const fakePasswords = [];
      for (let i = 0; i < 10; i++) {
        fakePasswords.push(FakeDataMaker.passwordEntry());
      }
      // Set list of passwords.
      this.passwordManager.lastCallback.addSavedPasswordListChangedListener(
          fakePasswords);
      Polymer.dom.flush();

      assertEquals(10, this.passwordsSection.savedPasswords.length);
    },
  },

  /** @override */
  violationFilter: SettingsAccessibilityTest.violationFilter,
});
