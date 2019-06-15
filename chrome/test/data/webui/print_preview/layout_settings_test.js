// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('layout_settings_test', function() {
  suite('LayoutSettingsTest', function() {
    let layoutSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      layoutSection = document.createElement('print-preview-layout-settings');
      layoutSection.settings = model.settings;
      layoutSection.disabled = false;
      test_util.fakeDataBind(model, layoutSection, 'settings');
      document.body.appendChild(layoutSection);
    });

    // Tests that setting the setting updates the UI.
    test('set setting', async () => {
      const select = layoutSection.$$('select');
      assertEquals('portrait', select.value);

      layoutSection.setSetting('layout', true);
      await test_util.eventToPromise('process-select-change', layoutSection);
      assertEquals('landscape', select.value);
    });

    // Tests that selecting a new option in the dropdown updates the setting.
    test('select option', async () => {
      // Verify that the selected option and names are as expected.
      const select = layoutSection.$$('select');
      assertEquals('portrait', select.value);
      assertFalse(layoutSection.getSettingValue('layout'));
      assertFalse(layoutSection.getSetting('layout').setFromUi);
      assertEquals(2, select.options.length);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(layoutSection, 'landscape');
      assertTrue(layoutSection.getSettingValue('layout'));
      assertTrue(layoutSection.getSetting('layout').setFromUi);
    });
  });
});
