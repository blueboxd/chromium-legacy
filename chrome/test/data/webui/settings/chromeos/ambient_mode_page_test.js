// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {settings.AmbientModeBrowserProxy}
 */
class TestAmbientModeBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'onAmbientModePageReady',
    ]);
  }

  /** @override */
  onAmbientModePageReady() {
    this.methodCalled('onAmbientModePageReady');
  }
}

suite('AmbientModeHandler', function() {
  /** @type {SettingsAmbientModePageElement} */
  let page = null;

  /** @type {?TestAmbientModeBrowserProxy} */
  let browserProxy = null;

  suiteSetup(function() {});

  setup(function() {
    browserProxy = new TestAmbientModeBrowserProxy();
    settings.AmbientModeBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      page = document.createElement('settings-ambient-mode-page');
      page.prefs = prefElement.prefs;
      document.body.appendChild(page);
    });
  });

  teardown(function() {
    page.remove();
  });

  test('toggleAmbientMode', function() {
    const button = page.$$('#ambientModeEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);

    // The button's state is set by the pref value.
    const enabled = page.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled, button.checked);

    // Click the button will toggle the pref value.
    button.click();
    Polymer.dom.flush();
    const enabled_toggled = page.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled_toggled, button.checked);
    assertEquals(enabled, !enabled_toggled);

    // Click again will toggle the pref value.
    button.click();
    Polymer.dom.flush();
    const enabled_toggled_twice =
        page.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled_toggled_twice, button.checked);
    assertEquals(enabled, enabled_toggled_twice);
  });

  test('chooseTopicSource', function() {
    const topicSourceRadioGroup = page.$$('#topicSourceRadioGroup');

    // The radio group's state is set by the pref value.
    let topicSourceValue =
        page.getPref('settings.ambient_mode.topic_source.value');
    assertEquals(topicSourceValue, parseFloat(topicSourceRadioGroup.selected));

    const radioButtons =
        topicSourceRadioGroup.querySelectorAll('cr-radio-button');
    assertEquals(2, radioButtons.length);

    // Click on topic source radio button will set the pref value.
    radioButtons.forEach(function(button, index) {
      button.click();
      topicSourceValue =
          page.getPref('settings.ambient_mode.topic_source.value');
      assertEquals(topicSourceValue, index);
    });
  });
});
