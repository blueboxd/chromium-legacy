// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.AppearanceBrowserProxy} */
class TestPersonalizationBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'isWallpaperSettingVisible',
      'isWallpaperPolicyControlled',
      'openWallpaperManager',
    ]);

    /** @private */
    this.isWallpaperSettingVisible_ = true;

    /** @private */
    this.isWallpaperPolicyControlled_ = false;
  }

  /** @override */
  isWallpaperSettingVisible() {
    this.methodCalled('isWallpaperSettingVisible');
    return Promise.resolve(this.isWallpaperSettingVisible_);
  }

  /** @override */
  isWallpaperPolicyControlled() {
    this.methodCalled('isWallpaperPolicyControlled');
    return Promise.resolve(this.isWallpaperPolicyControlled_);
  }

  /** @override */
  openWallpaperManager() {
    this.methodCalled('openWallpaperManager');
  }

  /** @param {boolean} Whether the wallpaper is policy controlled. */
  setIsWallpaperPolicyControlled(isPolicyControlled) {
    this.isWallpaperPolicyControlled_ = isPolicyControlled;
  }
}

let personalizationPage = null;

/** @type {?TestPersonalizationBrowserProxy} */
let personalizationBrowserProxy = null;

function createPersonalizationPage() {
  personalizationBrowserProxy.reset();
  PolymerTest.clearBody();

  personalizationPage = document.createElement('settings-personalization-page');
  personalizationPage.set('prefs', {
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

  personalizationPage.set('pageVisibility', {
    setWallpaper: true,
  });

  document.body.appendChild(personalizationPage);
  Polymer.dom.flush();
}

suite('PersonalizationHandler', function() {
  suiteSetup(function() {
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    personalizationBrowserProxy = new TestPersonalizationBrowserProxy();
    settings.PersonalizationBrowserProxyImpl.instance_ =
        personalizationBrowserProxy;
    createPersonalizationPage();
  });

  teardown(function() {
    personalizationPage.remove();
  });

  test('wallpaperManager', function() {
    personalizationBrowserProxy.setIsWallpaperPolicyControlled(false);
    // TODO(dschuyler): This should notice the policy change without needing
    // the page to be recreated.
    createPersonalizationPage();
    return personalizationBrowserProxy.whenCalled('isWallpaperPolicyControlled')
        .then(() => {
          const button = personalizationPage.$.wallpaperButton;
          assertTrue(!!button);
          assertFalse(button.disabled);
          button.click();
          return personalizationBrowserProxy.whenCalled('openWallpaperManager');
        });
  });

  test('wallpaperSettingVisible', function() {
    personalizationPage.set('pageVisibility.setWallpaper', false);
    return personalizationBrowserProxy.whenCalled('isWallpaperSettingVisible')
        .then(function() {
          Polymer.dom.flush();
          assertTrue(personalizationPage.$$('#wallpaperButton').hidden);
        });
  });

  test('wallpaperPolicyControlled', function() {
    // Should show the wallpaper policy indicator and disable the toggle
    // button if the wallpaper is policy controlled.
    personalizationBrowserProxy.setIsWallpaperPolicyControlled(true);
    createPersonalizationPage();
    return personalizationBrowserProxy.whenCalled('isWallpaperPolicyControlled')
        .then(function() {
          Polymer.dom.flush();
          assertFalse(
              personalizationPage.$$('#wallpaperPolicyIndicator').hidden);
          assertTrue(personalizationPage.$$('#wallpaperButton').disabled);
        });
  });

  test('changePicture', function() {
    const row = personalizationPage.$.changePictureRow;
    assertTrue(!!row);
    row.click();
    assertEquals(settings.routes.CHANGE_PICTURE, settings.getCurrentRoute());
  });
});
