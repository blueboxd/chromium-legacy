// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('pages_per_sheet_settings_test', function() {
  suite('PagesPerSheetSettingsTest', function() {
    let pagesPerSheetSection = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      pagesPerSheetSection =
          document.createElement('print-preview-pages-per-sheet-settings');
      pagesPerSheetSection.settings = model.settings;
      pagesPerSheetSection.disabled = false;
      test_util.fakeDataBind(model, pagesPerSheetSection, 'settings');
      document.body.appendChild(pagesPerSheetSection);
    });

    // Tests that setting the setting updates the UI.
    test('set setting', async () => {
      const select = pagesPerSheetSection.$$('select');
      assertEquals('1', select.value);

      pagesPerSheetSection.setSetting('pagesPerSheet', 4);
      await test_util.eventToPromise(
          'process-select-change', pagesPerSheetSection);
      assertEquals('4', select.value);
    });

    // Tests that setting the pages per sheet setting resets margins to DEFAULT.
    test('resets margins setting', async () => {
      pagesPerSheetSection.setSetting(
          'margins', print_preview.ticket_items.MarginsTypeValue.NO_MARGINS);
      assertEquals(1, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
      pagesPerSheetSection.setSetting('pagesPerSheet', 4);
      await test_util.eventToPromise(
          'process-select-change', pagesPerSheetSection);
      assertEquals(4, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
      assertEquals(
          print_preview.ticket_items.MarginsTypeValue.DEFAULT,
          pagesPerSheetSection.getSettingValue('margins'));
    });

    // Tests that selecting a new option in the dropdown updates the setting.
    test('select option', async () => {
      // Verify that the selected option and names are as expected.
      const select = pagesPerSheetSection.$$('select');
      assertEquals('1', select.value);
      assertEquals(1, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
      assertFalse(pagesPerSheetSection.getSetting('pagesPerSheet').setFromUi);
      assertEquals(6, select.options.length);

      // Verify that selecting an new option in the dropdown sets the setting.
      await print_preview_test_utils.selectOption(pagesPerSheetSection, '2');
      assertEquals(2, pagesPerSheetSection.getSettingValue('pagesPerSheet'));
      assertTrue(pagesPerSheetSection.getSetting('pagesPerSheet').setFromUi);
    });
  });
});
