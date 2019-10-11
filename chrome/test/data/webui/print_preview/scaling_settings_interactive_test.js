// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('scaling_settings_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    AutoFocusInput: 'auto focus input',
  };

  const suiteName = 'ScalingSettingsInteractiveTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewScalingSettingsElement} */
    let scalingSection = null;

    /** @type {?PrintPreviewModelElement} */
    let model = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);
      model.set('settings.scalingTypePdf.available', false);

      scalingSection = document.createElement('print-preview-scaling-settings');
      scalingSection.settings = model.settings;
      scalingSection.disabled = false;
      scalingSection.isPdf = false;
      test_util.fakeDataBind(model, scalingSection, 'settings');
      document.body.appendChild(scalingSection);
    });

    test(assert(TestNames.AutoFocusInput), async () => {
      const scalingInput =
          scalingSection.$$('print-preview-number-settings-section')
              .$.userValue.inputElement;
      const scalingDropdown = scalingSection.$$('.md-select');
      const collapse = scalingSection.$$('iron-collapse');

      assertFalse(collapse.opened);
      assertEquals(
          print_preview.ScalingType.DEFAULT,
          scalingSection.getSettingValue('scalingType'));

      // Select custom with the dropdown. This should autofocus the input.
      await Promise.all([
        print_preview_test_utils.selectOption(
            scalingSection, scalingSection.ScalingValue.CUSTOM.toString()),
        test_util.eventToPromise('transitionend', collapse),
      ]);
      assertTrue(collapse.opened);
      assertEquals(scalingInput, getDeepActiveElement());

      // Blur and select default.
      scalingInput.blur();
      await Promise.all([
        print_preview_test_utils.selectOption(
            scalingSection, scalingSection.ScalingValue.DEFAULT.toString()),
        test_util.eventToPromise('transitionend', collapse),
      ]);
      assertEquals(
          print_preview.ScalingType.DEFAULT,
          scalingSection.getSettingValue('scalingType'));
      assertFalse(scalingSection.$$('iron-collapse').opened);

      // Set custom in JS, which happens when we set the sticky settings. This
      // should not autofocus the input.
      scalingSection.setSetting(
          'scalingType', print_preview.ScalingType.CUSTOM);
      await test_util.eventToPromise('transitionend', collapse);
      assertTrue(collapse.opened);
      assertNotEquals(scalingInput, getDeepActiveElement());
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
