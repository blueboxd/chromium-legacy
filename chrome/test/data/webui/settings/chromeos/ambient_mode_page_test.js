// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {AmbientModeBrowserProxyImpl, CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

/**
 * @implements {settings.AmbientModeBrowserProxy}
 */
class TestAmbientModeBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestTopicSource',
      'requestAlbums',
      'setSelectedTopicSource',
      'setSelectedAlbums',
    ]);
  }

  /** @override */
  requestTopicSource() {
    this.methodCalled('requestTopicSource');
  }

  /** @override */
  requestAlbums(topicSource) {
    this.methodCalled('requestAlbums', [topicSource]);
  }

  /** @override */
  setSelectedTopicSource(topicSource) {
    this.methodCalled('setSelectedTopicSource', [topicSource]);
  }

  /** @override */
  setSelectedAlbums(settings) {
    this.methodCalled('setSelectedAlbums', [settings]);
  }
}

suite('AmbientModeHandler', function() {
  /** @type {SettingsAmbientModePageElement} */
  let ambientModePage = null;

  /** @type {SettingsAmbientModePhotosPageElement} */
  let ambientModePhotosPage = null;

  /** @type {?TestAmbientModeBrowserProxy} */
  let browserProxy = null;

  suiteSetup(function() {});

  setup(function() {
    browserProxy = new TestAmbientModeBrowserProxy();
    settings.AmbientModeBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    ambientModePhotosPage =
        document.createElement('settings-ambient-mode-photos-page');
    document.body.appendChild(ambientModePhotosPage);

    return CrSettingsPrefs.initialized.then(function() {
      ambientModePage = document.createElement('settings-ambient-mode-page');
      ambientModePage.prefs = prefElement.prefs;

      ambientModePage.prefs.settings.ambient_mode = {
        enabled: {value: true},
      };

      document.body.appendChild(ambientModePage);
      Polymer.dom.flush();
    });
  });

  teardown(function() {
    ambientModePage.remove();
    ambientModePhotosPage.remove();
  });

  test('toggleAmbientMode', function() {
    const button = ambientModePage.$$('#ambientModeEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);

    // The button's state is set by the pref value.
    const enabled =
        ambientModePage.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled, button.checked);

    // Click the button will toggle the pref value.
    button.click();
    Polymer.dom.flush();
    const enabled_toggled =
        ambientModePage.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled_toggled, button.checked);
    assertEquals(enabled, !enabled_toggled);

    // Click again will toggle the pref value.
    button.click();
    Polymer.dom.flush();
    const enabled_toggled_twice =
        ambientModePage.getPref('settings.ambient_mode.enabled.value');
    assertEquals(enabled_toggled_twice, button.checked);
    assertEquals(enabled, enabled_toggled_twice);
  });

  test('hasTopicSourceItems', function() {
    const topicSourceListElement = ambientModePage.$$('topic-source-list');
    const ironList = topicSourceListElement.$$('iron-list');
    const topicSourceItems = ironList.querySelectorAll('topic-source-item');
    assertEquals(2, topicSourceItems.length);
  });

  test('hasAlbums', function() {
    ambientModePhotosPage.albums_ = [
      {albumId: 'id0', checked: true, title: 'album0'},
      {albumId: 'id1', checked: false, title: 'album1'}
    ];
    Polymer.dom.flush();

    const ironList = ambientModePhotosPage.$$('iron-list');
    const checkboxes = ironList.querySelectorAll('cr-checkbox');
    assertEquals(2, checkboxes.length);

    const checkbox0 = checkboxes[0];
    const checkbox1 = checkboxes[1];
    assertEquals('id0', checkbox0.dataset.id);
    assertTrue(checkbox0.checked);
    assertEquals('album0', checkbox0.label);
    assertEquals('id1', checkbox1.dataset.id);
    assertFalse(checkbox1.checked);
    assertEquals('album1', checkbox1.label);
  });
});
