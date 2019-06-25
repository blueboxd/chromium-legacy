// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings_app_management_page', function() {
  suite('AppManagementUITest', function() {
    /** @type {SettingsAppManagementPageElement} */
    let appManagementPage = null;
    /** @type {settings.OpenWindowProxy} */
    let openWindowProxy = null;

    setup(function() {
      openWindowProxy = new TestOpenWindowProxy();
      settings.OpenWindowProxyImpl.instance_ = openWindowProxy;

      PolymerTest.clearBody();
      appManagementPage =
          document.createElement('settings-app-management-page');
      document.body.appendChild(appManagementPage);

      Polymer.dom.flush();
    });

    teardown(function() {
      appManagementPage.remove();
    });

    test('Open App Management', function() {
      // Hardcode this value so that the test is independent of the production
      // implementation that might include additional query parameters.
      const appManagementPageUrl = 'chrome://app-management';

      assertTrue(!!appManagementPage.$$('#appManagementButton'));
      appManagementPage.$$('#appManagementButton').click();
      Polymer.dom.flush();

      return openWindowProxy.whenCalled('openURL').then(url => {
        assertEquals(appManagementPageUrl, url);
      });
    });
  });
});
