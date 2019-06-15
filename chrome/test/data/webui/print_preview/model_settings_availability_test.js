// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('model_settings_availability_test', function() {
  suite('ModelSettingsAvailabilityTest', function() {
    let model = null;

    /** @override */
    setup(function() {
      PolymerTest.clearBody();
      model = document.createElement('print-preview-model');
      document.body.appendChild(model);

      model.documentSettings = {
        hasCssMediaStyles: false,
        hasSelection: false,
        isModifiable: true,
        isScalingDisabled: false,
        fitToPageScaling: 100,
        pageCount: 3,
        title: 'title',
      };

      model.pageSize = new print_preview.Size(612, 792);
      model.margins = new print_preview.Margins(72, 72, 72, 72);

      // Create a test destination.
      model.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      model.set(
          'destination.capabilities',
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities);
      model.applyStickySettings();
    });

    // These tests verify that the model correctly updates the settings
    // availability based on the destination and document info.
    test('copies', function() {
      assertTrue(model.settings.copies.available);

      // Remove copies capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.copies;
      model.set('destination.capabilities', capabilities);

      // Copies is no longer available.
      assertFalse(model.settings.copies.available);

      // Copies is restored.
      capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      model.set('destination.capabilities', capabilities);
      assertTrue(model.settings.copies.available);
      assertFalse(model.settings.copies.setFromUi);
    });

    test('collate', function() {
      assertTrue(model.settings.collate.available);

      // Remove collate capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.collate;
      model.set('destination.capabilities', capabilities);

      // Copies is no longer available.
      assertFalse(model.settings.collate.available);

      // Copies is restored.
      capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      model.set('destination.capabilities', capabilities);
      assertTrue(model.settings.collate.available);
      assertFalse(model.settings.collate.setFromUi);
    });

    test('layout', function() {
      // Layout is available since the printer has the capability and the
      // document is set to modifiable.
      assertTrue(model.settings.layout.available);

      // Each of these settings should not show the capability.
      [null,
       {option: [{type: 'PORTRAIT', is_default: true}]},
       {option: [{type: 'LANDSCAPE', is_default: true}]},
      ].forEach(layoutCap => {
        const capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        capabilities.printer.page_orientation = layoutCap;
        // Layout section should now be hidden.
        model.set('destination.capabilities', capabilities);
        assertFalse(model.settings.layout.available);
      });

      // Reset full capabilities
      const capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      model.set('destination.capabilities', capabilities);
      assertTrue(model.settings.layout.available);

      // Test with PDF - should be hidden.
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.layout.available);

      model.set('documentSettings.isModifiable', true);
      assertTrue(model.settings.layout.available);

      // Unavailable if document has CSS media styles.
      model.set('documentSettings.hasCssMediaStyles', true);
      assertFalse(model.settings.layout.available);
      assertFalse(model.settings.layout.setFromUi);
    });

    test('color', function() {
      // Color is available since the printer has the capability.
      assertTrue(model.settings.color.available);

      // Each of these settings should make the setting unavailable, with
      // |expectedValue| as its unavailableValue.
      [{
        colorCap: null,
        expectedValue: false,
      },
       {
         colorCap: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
         expectedValue: true,
       },
       {
         colorCap: {
           option: [
             {type: 'STANDARD_COLOR', is_default: true}, {type: 'CUSTOM_COLOR'}
           ]
         },
         expectedValue: true,
       },
       {
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME', is_default: true},
             {type: 'CUSTOM_MONOCHROME'}
           ]
         },
         expectedValue: false,
       },
       {
         colorCap: {option: [{type: 'STANDARD_MONOCHROME'}]},
         expectedValue: false,
       },
       {
         colorCap: {option: [{type: 'CUSTOM_MONOCHROME', vendor_id: '42'}]},
         expectedValue: false,
       },
       {
         colorCap: {option: [{type: 'CUSTOM_COLOR', vendor_id: '42'}]},
         expectedValue: true,
       }].forEach(capabilityAndValue => {
        const capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        capabilities.printer.color = capabilityAndValue.colorCap;
        model.set('destination.capabilities', capabilities);
        assertFalse(model.settings.color.available);
        assertEquals(
            capabilityAndValue.expectedValue,
            model.settings.color.unavailableValue);
      });

      // Each of these settings should make the setting available, with the
      // default value given by expectedValue.
      [{
        colorCap: {
          option: [
            {type: 'STANDARD_MONOCHROME', is_default: true},
            {type: 'STANDARD_COLOR'}
          ]
        },
        expectedValue: false,
      },
       {
         colorCap: {
           option: [
             {type: 'STANDARD_MONOCHROME'},
             {type: 'STANDARD_COLOR', is_default: true}
           ]
         },
         expectedValue: true,
       },
       {
         colorCap: {
           option: [
             {type: 'CUSTOM_MONOCHROME', vendor_id: '42'},
             {type: 'CUSTOM_COLOR', is_default: true, vendor_id: '43'}
           ]
         },
         expectedValue: true,
       }].forEach(capabilityAndValue => {
        const capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        capabilities.printer.color = capabilityAndValue.colorCap;
        model.set('destination.capabilities', capabilities);
        assertEquals(
            capabilityAndValue.expectedValue, model.settings.color.value);
        assertTrue(model.settings.color.available);
      });

      // Google Drive always has an unavailableValue of true.
      model.set(
          'destination',
          print_preview_test_utils.getGoogleDriveDestination(
              'foo@chromium.org'));
      const capabilities =
          print_preview_test_utils
              .getCddTemplate(print_preview.Destination.GooglePromotedId.DOCS)
              .capabilities;
      delete capabilities.printer.color;
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.color.available);
      assertTrue(model.settings.color.unavailableValue);
      assertFalse(model.settings.color.setFromUi);
    });

    function setSaveAsPdfDestination() {
      const saveAsPdf = print_preview_test_utils.getSaveAsPdfDestination();
      saveAsPdf.capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      model.set('destination', saveAsPdf);
    }

    test('media size', function() {
      // Media size is available since the printer has the capability.
      assertTrue(model.settings.mediaSize.available);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.media_size;

      // Section should now be hidden.
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.mediaSize.available);

      // Set Save as PDF printer.
      setSaveAsPdfDestination();

      // Save as PDF printer has media size capability.
      assertTrue(model.settings.mediaSize.available);

      // PDF to PDF -> media size is unavailable.
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.mediaSize.available);

      // CSS styles to PDF -> media size is unavailable.
      model.set('documentSettings.isModfiable', true);
      model.set('documentSettings.hasCssMediaStyles', true);
      assertFalse(model.settings.mediaSize.available);
      assertFalse(model.settings.color.setFromUi);
    });

    test('margins', function() {
      // The settings are available since isModifiable is true.
      assertTrue(model.settings.margins.available);
      assertTrue(model.settings.customMargins.available);

      // No margins settings for PDFs.
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.margins.available);
      assertFalse(model.settings.customMargins.available);
      assertFalse(model.settings.margins.setFromUi);
      assertFalse(model.settings.customMargins.setFromUi);
    });

    test('dpi', function() {
      // The settings are available since the printer has multiple DPI options.
      assertTrue(model.settings.dpi.available);

      // Remove capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.dpi;

      // Section should now be hidden.
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.dpi.available);

      // Does not show up for only 1 option. Unavailable value should be set to
      // the only available option.
      capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      capabilities.printer.dpi.option.pop();
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.dpi.available);
      assertEquals(200, model.settings.dpi.unavailableValue.horizontal_dpi);
      assertEquals(200, model.settings.dpi.unavailableValue.vertical_dpi);
      assertFalse(model.settings.dpi.setFromUi);
    });

    test('scaling', function() {
      // HTML -> printer
      assertTrue(model.settings.scaling.available);

      // HTML -> Save as PDF
      const defaultDestination = model.destination;
      setSaveAsPdfDestination();
      assertTrue(model.settings.scaling.available);

      // PDF -> Save as PDF
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.scaling.available);

      // PDF -> printer
      model.set('destination', defaultDestination);
      assertTrue(model.settings.scaling.available);
      assertFalse(model.settings.scaling.setFromUi);
    });

    test('fit to page', function() {
      // HTML -> printer
      assertFalse(model.settings.fitToPage.available);

      // HTML -> Save as PDF
      const defaultDestination = model.destination;
      setSaveAsPdfDestination();
      assertFalse(model.settings.fitToPage.available);

      // PDF -> Save as PDF
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.fitToPage.available);

      // PDF -> printer
      model.set('destination', defaultDestination);
      assertTrue(model.settings.fitToPage.available);
      assertFalse(model.settings.fitToPage.setFromUi);
    });

    test('header footer', function() {
      // Default margins + letter paper + HTML page.
      assertTrue(model.settings.headerFooter.available);

      // Set margins to NONE
      model.set(
          'settings.margins.value',
          print_preview.ticket_items.MarginsTypeValue.NO_MARGINS);
      assertFalse(model.settings.headerFooter.available);

      // Custom margins of 0.
      model.set(
          'settings.margins.value',
          print_preview.ticket_items.MarginsTypeValue.CUSTOM);
      model.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 0, marginRight: 0, marginBottom: 0});
      assertFalse(model.settings.headerFooter.available);

      // Custom margins of 36 -> header/footer available
      model.set(
          'settings.customMargins.value',
          {marginTop: 36, marginLeft: 36, marginRight: 36, marginBottom: 36});
      assertTrue(model.settings.headerFooter.available);

      // Zero top and bottom -> header/footer unavailable
      model.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 0});
      assertFalse(model.settings.headerFooter.available);

      // Zero top and nonzero bottom -> header/footer available
      model.set(
          'settings.customMargins.value',
          {marginTop: 0, marginLeft: 36, marginRight: 36, marginBottom: 36});
      assertTrue(model.settings.headerFooter.available);

      // Small paper sizes
      const capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      capabilities.printer.media_size = {
        'option': [
          {
            'name': 'SmallLabel',
            'width_microns': 38100,
            'height_microns': 12700,
            'is_default': false
          },
          {
            'name': 'BigLabel',
            'width_microns': 50800,
            'height_microns': 76200,
            'is_default': true
          }
        ]
      };
      model.set('destination.capabilities', capabilities);
      model.set(
          'settings.margins.value',
          print_preview.ticket_items.MarginsTypeValue.DEFAULT);

      // Header/footer should be available for default big label with
      // default margins.
      assertTrue(model.settings.headerFooter.available);

      model.set(
          'settings.mediaSize.value',
          capabilities.printer.media_size.option[0]);

      // Header/footer should not be available for small label
      assertFalse(model.settings.headerFooter.available);

      // Reset to big label.
      model.set(
          'settings.mediaSize.value',
          capabilities.printer.media_size.option[1]);
      assertTrue(model.settings.headerFooter.available);

      // Header/footer is never available for PDFs.
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.headerFooter.available);
      assertFalse(model.settings.headerFooter.setFromUi);
    });

    test('css background', function() {
      // The setting is available since isModifiable is true.
      assertTrue(model.settings.cssBackground.available);

      // No CSS background setting for PDFs.
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.cssBackground.available);
      assertFalse(model.settings.cssBackground.setFromUi);
    });

    test('duplex', function() {
      assertTrue(model.settings.duplex.available);
      assertTrue(model.settings.duplexShortEdge.available);

      // Remove duplex capability.
      let capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.duplex;
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.duplex.available);
      assertFalse(model.settings.duplexShortEdge.available);

      // Set a duplex capability with only 1 type, no duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.duplex;
      capabilities.printer.duplex = {
        option: [{type: print_preview.DuplexType.NO_DUPLEX, is_default: true}]
      };
      model.set('destination.capabilities', capabilities);
      assertFalse(model.settings.duplex.available);
      assertFalse(model.settings.duplexShortEdge.available);

      // Set a duplex capability with 2 types, long edge and no duplex.
      capabilities =
          print_preview_test_utils.getCddTemplate(model.destination.id)
              .capabilities;
      delete capabilities.printer.duplex;
      capabilities.printer.duplex = {
        option: [
          {type: print_preview.DuplexType.NO_DUPLEX},
          {type: print_preview.DuplexType.LONG_EDGE, is_default: true}
        ]
      };
      model.set('destination.capabilities', capabilities);
      assertTrue(model.settings.duplex.available);
      assertFalse(model.settings.duplexShortEdge.available);
      assertFalse(model.settings.duplex.setFromUi);
      assertFalse(model.settings.duplexShortEdge.setFromUi);
    });

    test('rasterize', function() {
      assertFalse(model.settings.rasterize.available);

      // Available on non-Windows and Mac for PDFs.
      model.set('documentSettings.isModifiable', false);
      assertEquals(
          !cr.isWindows && !cr.isMac, model.settings.rasterize.available);
      assertFalse(model.settings.rasterize.setFromUi);
    });

    test('selection only', function() {
      // Not available with no selection.
      assertFalse(model.settings.selectionOnly.available);

      model.set('documentSettings.hasSelection', true);
      assertTrue(model.settings.selectionOnly.available);

      // Not available for PDFs.
      model.set('documentSettings.isModifiable', false);
      assertFalse(model.settings.selectionOnly.available);
      assertFalse(model.settings.selectionOnly.setFromUi);
    });

    if (cr.isChromeOS) {
      test('pin', function() {
        // Make device unmanaged.
        loadTimeData.overrideValues({isEnterpriseManaged: false});
        // Check that pin setting is unavailable on unmanaged devices.
        assertFalse(model.settings.pin.available);

        // Make device enterprise managed.
        loadTimeData.overrideValues({isEnterpriseManaged: true});
        // Set capabilities again to update pin availability.
        model.set(
            'destination.capabilities',
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities);
        assertTrue(model.settings.pin.available);

        // Remove pin capability.
        let capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        delete capabilities.printer.pin;
        model.set('destination.capabilities', capabilities);
        assertFalse(model.settings.pin.available);

        // Set not supported pin capability.
        capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        capabilities.printer.pin.supported = false;
        model.set('destination.capabilities', capabilities);
        assertFalse(model.settings.pin.available);
        assertFalse(model.settings.pin.setFromUi);
      });

      test('pinValue', function() {
        assertTrue(model.settings.pinValue.available);

        // Remove pin capability.
        let capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        delete capabilities.printer.pin;
        model.set('destination.capabilities', capabilities);
        assertFalse(model.settings.pinValue.available);

        // Set not supported pin capability.
        capabilities =
            print_preview_test_utils.getCddTemplate(model.destination.id)
                .capabilities;
        capabilities.printer.pin.supported = false;
        model.set('destination.capabilities', capabilities);
        assertFalse(model.settings.pinValue.available);
        assertFalse(model.settings.pinValue.setFromUi);
      });
    }
  });
});
