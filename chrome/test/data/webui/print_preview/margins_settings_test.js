// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('margins_settings_test', function() {
  suite('MarginsSettingsTest', function() {
    let marginsSection = null;

    let marginsTypeEnum = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      marginsSection = document.createElement('print-preview-margins-settings');
      document.body.appendChild(marginsSection);
      marginsSection.settings = model.settings;
      marginsSection.disabled = false;
      test_util.fakeDataBind(model, marginsSection, 'settings');
      marginsTypeEnum = print_preview.ticket_items.MarginsTypeValue;
    });

    // Tests that setting the setting updates the UI.
    test('set setting', async () => {
      const select = marginsSection.$$('select');
      assertEquals(marginsTypeEnum.DEFAULT.toString(), select.value);

      marginsSection.setSetting('margins', marginsTypeEnum.MINIMUM);
      await test_util.eventToPromise('process-select-change', marginsSection);
      assertEquals(marginsTypeEnum.MINIMUM.toString(), select.value);
    });

    // Tests that selecting a new option in the dropdown updates the setting.
    test('select option', async () => {
      // Verify that the selected option and names are as expected.
      const select = marginsSection.$$('select');
      assertEquals(marginsTypeEnum.DEFAULT.toString(), select.value);
      assertEquals(
          marginsTypeEnum.DEFAULT, marginsSection.getSettingValue('margins'));
      assertEquals(4, select.options.length);
      assertFalse(marginsSection.getSetting('margins').setFromUi);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(
          marginsSection, marginsTypeEnum.MINIMUM.toString());
      assertEquals(
          marginsTypeEnum.MINIMUM, marginsSection.getSettingValue('margins'));
      assertTrue(marginsSection.getSetting('margins').setFromUi);
    });

    // This test verifies that changing pages per sheet to N > 1 disables the
    // margins dropdown.
    test('disabled by pages per sheet', function() {
      const select = marginsSection.$$('select');
      assertFalse(select.disabled);

      marginsSection.setSetting('pagesPerSheet', 2);
      assertTrue(select.disabled);

      marginsSection.setSetting('pagesPerSheet', 1);
      assertFalse(select.disabled);
    });
  });
});
