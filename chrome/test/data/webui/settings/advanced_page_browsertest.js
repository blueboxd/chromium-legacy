// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings advanced page. */

// Polymer BrowserTest fixture.
GEN_INCLUDE(['//chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * @constructor
 * @extends {PolymerTest}
 */
function SettingsAdvancedPageBrowserTest() {}

SettingsAdvancedPageBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://settings/',

  /** @override */
  extraLibraries: [
    ...PolymerTest.prototype.extraLibraries,
    'settings_page_test_util.js',
  ],

  /** @override */
  setUp: function() {
    PolymerTest.prototype.setUp.call(this);
    suiteSetup(function() {
      return CrSettingsPrefs.initialized;
    });

    suiteSetup(() => {
      return settings_page_test_util.getPage('basic').then(basicPage => {
        this.basicPage = basicPage;
      });
    });
  },

  /**
   * Verifies the section has a visible #main element and that any possible
   * sub-pages are hidden.
   * @param {!Node} The DOM node for the section.
   */
  verifySubpagesHidden: function(section) {
    // Check if there are sub-pages to verify.
    const pages = section.firstElementChild.shadowRoot.querySelector(
        'settings-animated-pages');
    if (!pages) {
      return;
    }

    const children = pages.getContentChildren();
    const stampedChildren = children.filter(function(element) {
      return element.tagName != 'TEMPLATE';
    });

    // The section's main child should be stamped and visible.
    const main = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') == 'default';
    });
    assertEquals(
        main.length, 1,
        'default card not found for section ' + section.section);
    assertGT(main[0].offsetHeight, 0);

    // Any other stamped subpages should not be visible.
    const subpages = stampedChildren.filter(function(element) {
      return element.getAttribute('route-path') != 'default';
    });
    for (const subpage of subpages) {
      assertEquals(
          subpage.offsetHeight, 0,
          'Expected subpage #' + subpage.id + ' in ' + section.section +
              ' not to be visible.');
    }
  }
};

// Times out on debug builders because the Settings page can take several
// seconds to load in a Release build and several times that in a Debug build.
// See https://crbug.com/558434.
GEN('#if !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');

TEST_F('SettingsAdvancedPageBrowserTest', 'MAYBE_Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  const self = this;

  // Register mocha tests.
  suite('SettingsPage', function() {
    suiteSetup(function() {
      const settingsMain =
          document.querySelector('settings-ui').$$('settings-main');
      assert(!!settingsMain);
      settingsMain.advancedToggleExpanded =
          !settingsMain.advancedToggleExpanded;
      Polymer.dom.flush();
    });

    test('load page', function() {
      // This will fail if there are any asserts or errors in the Settings page.
    });

    test('advanced pages', function() {
      const page = self.basicPage;
      const sections =
          ['privacy', 'languages', 'downloads', 'printing', 'reset'];
      for (let i = 0; i < sections.length; i++) {
        const section = settings_page_test_util.getSection(page, sections[i]);
        assertTrue(!!section);
        self.verifySubpagesHidden(section);
      }
    });
  });

  // Run all registered tests.
  mocha.run();
});
