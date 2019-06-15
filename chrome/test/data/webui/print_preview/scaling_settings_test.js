// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('scaling_settings_test', function() {
  /** @enum {string} */
  const TestNames = {
    ShowCorrectDropdownOptions: 'show correct dropdown options',
    SetScaling: 'set scaling',
    InputNotDisabledOnValidityChange: 'input not disabled on validity change',
  };

  const suiteName = 'ScalingSettingsTest';
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
      model.set('settings.fitToPage.available', false);

      scalingSection = document.createElement('print-preview-scaling-settings');
      scalingSection.settings = model.settings;
      scalingSection.disabled = false;
      test_util.fakeDataBind(model, scalingSection, 'settings');
      document.body.appendChild(scalingSection);
    });

    test(assert(TestNames.ShowCorrectDropdownOptions), function() {
      // Fit to page unavailable -> No fit to page option.
      const fitToPageOption = scalingSection.$$(
          `[value="${scalingSection.ScalingValue.FIT_TO_PAGE}"]`);
      const defaultOption =
          scalingSection.$$(`[value="${scalingSection.ScalingValue.DEFAULT}"]`);
      const customOption =
          scalingSection.$$(`[value="${scalingSection.ScalingValue.CUSTOM}"]`);
      assertTrue(fitToPageOption.hidden);
      assertFalse(defaultOption.hidden);
      assertFalse(customOption.hidden);

      // Fit to page available -> All 3 options.
      model.set('settings.fitToPage.available', true);
      assertFalse(fitToPageOption.hidden);
      assertFalse(defaultOption.hidden);
      assertFalse(customOption.hidden);
    });

    /**
     * @param {string} expectedScaling The expected scaling value.
     * @param {boolean} valid Whether the scaling setting is valid.
     * @param {boolean} isCustom Whether custom scaling should be set.
     * @param {boolean} fitToPage Expected fit to page value.
     * @param {string} scalingDisplayValue The value that should be displayed in
     *     the UI for scaling.
     */
    function validateState(
        expectedScaling, valid, isCustom, fitToPage, scalingDisplayValue) {
      // Validate the settings were set as expected.
      assertEquals(expectedScaling, scalingSection.getSettingValue('scaling'));
      assertEquals(valid, scalingSection.getSetting('scaling').valid);
      assertEquals(fitToPage, scalingSection.getSettingValue('fitToPage'));
      assertEquals(isCustom, scalingSection.getSettingValue('customScaling'));

      // Validate UI values that are set by JS.
      const scalingInput =
          scalingSection.$$('print-preview-number-settings-section').getInput();
      const collapse = scalingSection.$$('iron-collapse');
      assertEquals(!valid, scalingInput.invalid);
      assertEquals(scalingDisplayValue, scalingInput.value);
      assertEquals(isCustom && !fitToPage, collapse.opened);
    }

    // Verifies that setting the scaling value using the dropdown and/or the
    // custom input works correctly.
    test(assert(TestNames.SetScaling), async () => {
      // Default is 100
      const scalingInput =
          scalingSection.$$('print-preview-number-settings-section')
              .$.userValue.inputElement;
      const scalingDropdown = scalingSection.$$('.md-select');

      // Make fit to page available.
      model.set('settings.fitToPage.available', true);

      // Default is 100
      validateState('100', true, false, false, '100');
      assertFalse(scalingSection.getSetting('scaling').setFromUi);
      assertFalse(scalingSection.getSetting('customScaling').setFromUi);
      assertFalse(scalingSection.getSetting('fitToPage').setFromUi);

      // Select custom
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.CUSTOM.toString());
      validateState('100', true, true, false, '100');
      assertTrue(scalingSection.getSetting('customScaling').setFromUi);
      assertTrue(scalingSection.getSetting('fitToPage').setFromUi);

      await print_preview_test_utils.triggerInputEvent(
          scalingInput, '105', scalingSection);
      validateState('105', true, true, false, '105');
      assertTrue(scalingSection.getSetting('scaling').setFromUi);

      // Change to fit to page.
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.FIT_TO_PAGE.toString());
      validateState('105', true, true, true, '105');

      // Go back to custom. Restores 105 value.
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.CUSTOM.toString());
      validateState('105', true, true, false, '105');

      // Set scaling to something invalid. Should change setting validity
      // but not value.
      await print_preview_test_utils.triggerInputEvent(
          scalingInput, '5', scalingSection);
      validateState('105', false, true, false, '5');

      // Select fit to page. Should clear the invalid value.
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.FIT_TO_PAGE.toString());
      validateState('105', true, true, true, '105');

      // Custom scaling should set to last valid.
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.CUSTOM.toString());
      validateState('105', true, true, false, '105');

      // Set scaling to something invalid. Should change setting validity
      // but not value.
      await print_preview_test_utils.triggerInputEvent(
          scalingInput, '500', scalingSection);
      validateState('105', false, true, false, '500');

      // Pick default scaling. This should clear the error.
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.DEFAULT.toString());
      validateState('105', true, false, false, '105');

      // Custom scaling should set to last valid.
      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.CUSTOM.toString());
      validateState('105', true, true, false, '105');

      // Enter a blank value in the scaling field. This should not
      // change the stored value of scaling or fit to page, to avoid an
      // unnecessary preview regeneration.
      await print_preview_test_utils.triggerInputEvent(
          scalingInput, '', scalingSection);
      validateState('105', true, true, false, '');
    });


    // Verifies that the input is never disabled when the validity of the
    // setting changes.
    test(assert(TestNames.InputNotDisabledOnValidityChange), async () => {
      const numberSection =
          scalingSection.$$('print-preview-number-settings-section');
      const input = numberSection.getInput();

      // In the real UI, the print preview app listens for this event from this
      // section and others and sets disabled to true if any change from true to
      // false is detected. Imitate this here. Since we are only interacting
      // with the scaling input, at no point should the input be disabled, as it
      // will lose focus.
      model.addEventListener('setting-valid-changed', function(e) {
        assertFalse(input.disabled);
      });

      await print_preview_test_utils.selectOption(
          scalingSection, scalingSection.ScalingValue.CUSTOM.toString());
      await print_preview_test_utils.triggerInputEvent(
          input, '90', scalingSection);
      validateState('90', true, true, false, '90');

      // Set invalid input
      await print_preview_test_utils.triggerInputEvent(
          input, '9', scalingSection);
      validateState('90', false, true, false, '9');

      // Restore valid input
      await print_preview_test_utils.triggerInputEvent(
          input, '90', scalingSection);
      validateState('90', true, true, false, '90');

      // Invalid input again
      await print_preview_test_utils.triggerInputEvent(
          input, '9', scalingSection);
      validateState('90', false, true, false, '9');

      // Clear input
      await print_preview_test_utils.triggerInputEvent(
          input, '', scalingSection);
      validateState('90', true, true, false, '');

      // Set valid input
      await print_preview_test_utils.triggerInputEvent(
          input, '50', scalingSection);
      validateState('50', true, true, false, '50');
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
