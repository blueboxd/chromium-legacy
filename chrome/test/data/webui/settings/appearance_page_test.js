// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {isChromeOS, isLinux} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
// clang-format on

/** @implements {AppearanceBrowserProxy} */
class TestAppearanceBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getDefaultZoom',
      'getThemeInfo',
      'isSupervised',
      'useDefaultTheme',
      'useSystemTheme',
      'validateStartupPage',
    ]);

    /** @private */
    this.defaultZoom_ = 1;

    /** @private */
    this.isSupervised_ = false;

    /** @private */
    this.isHomeUrlValid_ = true;
  }

  /** @override */
  getDefaultZoom() {
    this.methodCalled('getDefaultZoom');
    return Promise.resolve(this.defaultZoom_);
  }

  /** @override */
  getThemeInfo(themeId) {
    this.methodCalled('getThemeInfo', themeId);
    return Promise.resolve({name: 'Sports car red'});
  }

  /** @override */
  isSupervised() {
    this.methodCalled('isSupervised');
    return this.isSupervised_;
  }

  /** @override */
  useDefaultTheme() {
    this.methodCalled('useDefaultTheme');
  }

  /** @override */
  useSystemTheme() {
    this.methodCalled('useSystemTheme');
  }

  /** @param {number} defaultZoom */
  setDefaultZoom(defaultZoom) {
    this.defaultZoom_ = defaultZoom;
  }

  /** @param {boolean} Whether the user is supervised */
  setIsSupervised(isSupervised) {
    this.isSupervised_ = isSupervised;
  }

  /** @override */
  validateStartupPage(url) {
    this.methodCalled('validateStartupPage', url);
    return Promise.resolve(this.isHomeUrlValid_);
  }

  /**
   * @param {boolean} isValid
   */
  setValidStartupPageResponse(isValid) {
    this.isHomeUrlValid_ = isValid;
  }
}

let appearancePage = null;

/** @type {?TestAppearanceBrowserProxy} */
let appearanceBrowserProxy = null;

function createAppearancePage() {
  appearanceBrowserProxy.reset();
  PolymerTest.clearBody();

  appearancePage = document.createElement('settings-appearance-page');
  appearancePage.set('prefs', {
    extensions: {
      theme: {
        id: {
          value: '',
        },
        use_system: {
          value: false,
        },
      },
    },
  });

  appearancePage.set('pageVisibility', {
    setWallpaper: true,
  });

  document.body.appendChild(appearancePage);
  flush();
}

suite('AppearanceHandler', function() {
  setup(function() {
    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    AppearanceBrowserProxyImpl.instance_ = appearanceBrowserProxy;
    createAppearancePage();
  });

  teardown(function() {
    appearancePage.remove();
  });

  const THEME_ID_PREF = 'prefs.extensions.theme.id.value';

  if (isLinux && !isChromeOS) {
    const USE_SYSTEM_PREF = 'prefs.extensions.theme.use_system.value';

    test('useDefaultThemeLinux', function() {
      assertFalse(!!appearancePage.get(THEME_ID_PREF));
      assertFalse(appearancePage.get(USE_SYSTEM_PREF));
      // No custom nor system theme in use; "USE CLASSIC" should be hidden.
      assertFalse(!!appearancePage.$$('#useDefault'));

      appearancePage.set(USE_SYSTEM_PREF, true);
      flush();
      // If the system theme is in use, "USE CLASSIC" should show.
      assertTrue(!!appearancePage.$$('#useDefault'));

      appearancePage.set(USE_SYSTEM_PREF, false);
      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      flush();

      // With a custom theme installed, "USE CLASSIC" should show.
      const button = appearancePage.$$('#useDefault');
      assertTrue(!!button);

      button.click();
      return appearanceBrowserProxy.whenCalled('useDefaultTheme');
    });

    test('useSystemThemeLinux', function() {
      assertFalse(!!appearancePage.get(THEME_ID_PREF));
      appearancePage.set(USE_SYSTEM_PREF, true);
      flush();
      // The "USE GTK+" button shouldn't be showing if it's already in use.
      assertFalse(!!appearancePage.$$('#useSystem'));

      appearanceBrowserProxy.setIsSupervised(true);
      appearancePage.set(USE_SYSTEM_PREF, false);
      flush();
      // Supervised users have their own theme and can't use GTK+ theme.
      assertFalse(!!appearancePage.$$('#useDefault'));
      assertFalse(!!appearancePage.$$('#useSystem'));
      // If there's no "USE" buttons, the container should be hidden.
      assertTrue(appearancePage.$$('#themesSecondaryActions').hidden);

      appearanceBrowserProxy.setIsSupervised(false);
      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      flush();
      // If there's "USE" buttons again, the container should be visible.
      assertTrue(!!appearancePage.$$('#useDefault'));
      assertFalse(appearancePage.$$('#themesSecondaryActions').hidden);

      const button = appearancePage.$$('#useSystem');
      assertTrue(!!button);

      button.click();
      return appearanceBrowserProxy.whenCalled('useSystemTheme');
    });
  } else {
    test('useDefaultTheme', function() {
      assertFalse(!!appearancePage.get(THEME_ID_PREF));
      assertFalse(!!appearancePage.$$('#useDefault'));

      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      flush();

      // With a custom theme installed, "RESET TO DEFAULT" should show.
      const button = appearancePage.$$('#useDefault');
      assertTrue(!!button);

      button.click();
      return appearanceBrowserProxy.whenCalled('useDefaultTheme');
    });
  }

  test('default zoom handling', function() {
    function getDefaultZoomText() {
      const zoomLevel = appearancePage.$.zoomLevel;
      return zoomLevel.options[zoomLevel.selectedIndex].textContent.trim();
    }

    return appearanceBrowserProxy.whenCalled('getDefaultZoom')
        .then(function() {
          assertEquals('100%', getDefaultZoomText());

          appearanceBrowserProxy.setDefaultZoom(2 / 3);
          createAppearancePage();
          return appearanceBrowserProxy.whenCalled('getDefaultZoom');
        })
        .then(function() {
          assertEquals('67%', getDefaultZoomText());

          appearanceBrowserProxy.setDefaultZoom(11 / 10);
          createAppearancePage();
          return appearanceBrowserProxy.whenCalled('getDefaultZoom');
        })
        .then(function() {
          assertEquals('110%', getDefaultZoomText());

          appearanceBrowserProxy.setDefaultZoom(1.7499999999999);
          createAppearancePage();
          return appearanceBrowserProxy.whenCalled('getDefaultZoom');
        })
        .then(function() {
          assertEquals('175%', getDefaultZoomText());
        });
  });

  test('show home button toggling', function() {
    assertFalse(!!appearancePage.$$('.list-frame'));
    appearancePage.set('prefs', {
      browser: {show_home_button: {value: true}},
      extensions: {theme: {id: {value: ''}}},
    });
    flush();

    assertTrue(!!appearancePage.$$('.list-frame'));
  });
});

suite('HomeUrlInput', function() {
  let homeUrlInput;

  setup(function() {
    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    AppearanceBrowserProxyImpl.instance_ = appearanceBrowserProxy;
    PolymerTest.clearBody();

    homeUrlInput = document.createElement('home-url-input');
    homeUrlInput.set(
        'pref', {type: chrome.settingsPrivate.PrefType.URL, value: 'test'});

    document.body.appendChild(homeUrlInput);
    flush();
  });

  test('home button urls', function() {
    assertFalse(homeUrlInput.invalid);
    assertEquals(homeUrlInput.value, 'test');

    homeUrlInput.value = '@@@';
    appearanceBrowserProxy.setValidStartupPageResponse(false);
    homeUrlInput.$.input.fire('input');

    return appearanceBrowserProxy.whenCalled('validateStartupPage')
        .then(function(url) {
          assertEquals(homeUrlInput.value, url);
          flush();
          assertEquals(homeUrlInput.value, '@@@');  // Value hasn't changed.
          assertTrue(homeUrlInput.invalid);

          // Should reset to default value on change event.
          homeUrlInput.$.input.fire('change');
          flush();
          assertEquals(homeUrlInput.value, 'test');
        });
  });
});
