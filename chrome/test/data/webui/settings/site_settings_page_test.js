// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('SiteSettingsPage', function() {
  /** @type {settings.TestMetricsBrowserProxy} */
  let testBrowserProxy;

  /** @type {SettingsSiteSettingsPageElement} */
  let page;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      privacySettingsRedesignEnabled: false,
    });
  });

  function setupPage() {
    testBrowserProxy = new TestMetricsBrowserProxy();
    settings.MetricsBrowserProxyImpl.instance_ = testBrowserProxy;
    PolymerTest.clearBody();
    page = document.createElement('settings-site-settings-page');
    document.body.appendChild(page);
    Polymer.dom.flush();
  }

  setup(setupPage);

  teardown(function() {
    page.remove();
  });

  test('DefaultLabels', function() {
    assertEquals(
        'a',
        settings.defaultSettingLabel(settings.ContentSetting.ALLOW, 'a', 'b'));
    assertEquals(
        'b',
        settings.defaultSettingLabel(settings.ContentSetting.BLOCK, 'a', 'b'));
    assertEquals(
        'a',
        settings.defaultSettingLabel(
            settings.ContentSetting.ALLOW, 'a', 'b', 'c'));
    assertEquals(
        'b',
        settings.defaultSettingLabel(
            settings.ContentSetting.BLOCK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.SESSION_ONLY, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.DEFAULT, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.ASK, 'a', 'b', 'c'));
    assertEquals(
        'c',
        settings.defaultSettingLabel(
            settings.ContentSetting.IMPORTANT_CONTENT, 'a', 'b', 'c'));
  });
});
