// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('preview_area_test', function() {
  /** @enum {string} */
  const TestNames = {
    StateChanges: 'state changes',
    ViewportSizeChanges: 'viewport size changes',
  };

  const suiteName = 'PreviewAreaTest';
  suite(suiteName, function() {
    /** @type {?PrintPreviewPreviewAreaElement} */
    let previewArea = null;

    /** @type {?print_preview.NativeLayer} */
    let nativeLayer = null;

    let pluginProxy = null;

    /** @override */
    setup(function() {
      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      nativeLayer.setPageCount(3);
      pluginProxy = new print_preview.PDFPluginStub();
      print_preview.PluginProxy.setInstance(pluginProxy);

      PolymerTest.clearBody();
      const model = document.createElement('print-preview-model');
      document.body.appendChild(model);
      model.setSetting('pages', [1, 2, 3]);
      previewArea = document.createElement('print-preview-preview-area');
      document.body.appendChild(previewArea);
      previewArea.settings = model.settings;
      test_util.fakeDataBind(model, previewArea, 'settings');
      previewArea.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.LOCAL,
          print_preview.DestinationOrigin.LOCAL, 'FooName',
          print_preview.DestinationConnectionStatus.ONLINE);
      previewArea.destination.capabiliites =
          print_preview_test_utils.getCddTemplate('FooDevice');
      previewArea.error = print_preview.Error.NONE;
      previewArea.state = print_preview.State.NOT_READY;
      previewArea.documentModifiable = true;
      previewArea.measurementSystem = new print_preview.MeasurementSystem(
          ',', '.', print_preview.MeasurementSystemUnitType.IMPERIAL);

      previewArea.pageSize = new print_preview.Size(612, 794);
      previewArea.margins = new print_preview.Margins(10, 10, 10, 10);
    });

    /** Validate some preview area state transitions work as expected. */
    test(assert(TestNames.StateChanges), function() {
      // Simulate starting the preview.
      const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
      previewArea.state = print_preview.State.READY;
      assertEquals(
          print_preview.PreviewAreaState.LOADING, previewArea.previewState);
      assertFalse(previewArea.$$('.preview-area-overlay-layer')
                      .classList.contains('invisible'));
      const message =
          previewArea.$$('.preview-area-message').querySelector('span');
      assertEquals('Loading preview', message.textContent.trim());

      previewArea.startPreview();

      return whenPreviewStarted.then(() => {
        assertEquals(
            print_preview.PreviewAreaState.DISPLAY_PREVIEW,
            previewArea.previewState);
        assertEquals(3, pluginProxy.getCallCount('loadPreviewPage'));
        assertTrue(previewArea.$$('.preview-area-overlay-layer')
                       .classList.contains('invisible'));

        // If destination capabilities fetch fails, the invalid printer error
        // will be set by the destination settings.
        previewArea.destination = new print_preview.Destination(
            'InvalidDevice', print_preview.DestinationType.LOCAL,
            print_preview.DestinationOrigin.LOCAL, 'InvalidName',
            print_preview.DestinationConnectionStatus.ONLINE);
        previewArea.state = print_preview.State.ERROR;
        previewArea.error = print_preview.Error.INVALID_PRINTER;
        assertEquals(
            print_preview.PreviewAreaState.ERROR, previewArea.previewState);
        assertFalse(previewArea.$$('.preview-area-overlay-layer')
                        .classList.contains('invisible'));
        assertEquals(
            'The selected printer is not available or not installed ' +
                'correctly.  Check your printer or try selecting another ' +
                'printer.',
            message.textContent.trim());
      });
    });

    /** Validate preview area sets tabindex correctly based on viewport size. */
    test(assert(TestNames.ViewportSizeChanges), function() {
      // Simulate starting the preview.
      const whenPreviewStarted = nativeLayer.whenCalled('getPreview');
      previewArea.state = print_preview.State.READY;
      previewArea.startPreview();

      return whenPreviewStarted.then(() => {
        assertEquals(
            print_preview.PreviewAreaState.DISPLAY_PREVIEW,
            previewArea.previewState);
        assertTrue(previewArea.$$('.preview-area-overlay-layer')
                       .classList.contains('invisible'));
        const plugin = previewArea.$$('.preview-area-plugin');
        assertEquals(null, plugin.getAttribute('tabindex'));

        // This can be triggered at any time by a resizing of the viewport or
        // change to the PDF viewer zoom.
        // Plugin is too narrow to show zoom toolbar.
        pluginProxy.triggerVisualStateChange(0, 0, 150, 150, 500);
        assertEquals('-1', plugin.getAttribute('tabindex'));
        // Plugin is large enough for zoom toolbar.
        pluginProxy.triggerVisualStateChange(0, 0, 250, 400, 500);
        assertEquals('0', plugin.getAttribute('tabindex'));
        // Plugin is too short for zoom toolbar.
        pluginProxy.triggerVisualStateChange(0, 0, 250, 400, 100);
        assertEquals('-1', plugin.getAttribute('tabindex'));
        // Plugin is large enough for zoom toolbar.
        pluginProxy.triggerVisualStateChange(0, 0, 500, 800, 1000);
        assertEquals('0', plugin.getAttribute('tabindex'));
      });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
