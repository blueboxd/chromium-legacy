// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {LOTTIE_JS_URL} from 'chrome://resources/cr_elements/cr_lottie/cr_lottie.m.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {eventToPromise} from '../test_util.m.js';
// #import {MockController, MockMethod} from '../mock_controller.m.js';
// clang-format on

/** @fileoverview Suite of tests for cr-lottie. */
suite('cr_lottie_test', function() {
  /**
   * A data url that produces a sample solid green json lottie animation.
   * @type {string}
   */
  const SAMPLE_LOTTIE_GREEN =
      'data:application/json;base64,eyJ2IjoiNC42LjkiLCJmciI6NjAsImlwIjowLCJvc' +
      'CI6MjAwLCJ3Ijo4MDAsImgiOjYwMCwiZGRkIjowLCJhc3NldHMiOltdLCJsYXllcnMiOlt' +
      '7ImluZCI6MSwidHkiOjEsInNjIjoiIzAwZmYwMCIsImFvIjowLCJpcCI6MCwib3AiOjIwM' +
      'Cwic3QiOjAsInNyIjoxLCJzdyI6ODAwLCJzaCI6NjAwLCJibSI6MCwia3MiOnsibyI6eyJ' +
      'hIjowLCJrIjoxMDB9LCJyIjp7ImEiOjAsImsiOlswLDAsMF19LCJwIjp7ImEiOjAsImsiO' +
      'lszMDAsMjAwLDBdfSwiYSI6eyJhIjowLCJrIjpbMzAwLDIwMCwwXX0sInMiOnsiYSI6MCw' +
      'iayI6WzEwMCwxMDAsMTAwXX19fV19';

  /**
   * A data url that produces a sample solid blue json lottie animation.
   * @type {string}
   */
  const SAMPLE_LOTTIE_BLUE =
      'data:application/json;base64,eyJhc3NldHMiOltdLCJkZGQiOjAsImZyIjo2MCwia' +
      'CI6NjAwLCJpcCI6MCwibGF5ZXJzIjpbeyJhbyI6MCwiYm0iOjAsImluZCI6MSwiaXAiOjA' +
      'sImtzIjp7ImEiOnsiYSI6MCwiayI6WzMwMCwyMDAsMF19LCJvIjp7ImEiOjAsImsiOjEwM' +
      'H0sInAiOnsiYSI6MCwiayI6WzMwMCwyMDAsMF19LCJyIjp7ImEiOjAsImsiOlswLDAsMF1' +
      '9LCJzIjp7ImEiOjAsImsiOlsxMDAsMTAwLDEwMF19fSwib3AiOjIwMCwic2MiOiIjMDAwM' +
      'GZmIiwic2giOjYwMCwic3IiOjEsInN0IjowLCJzdyI6ODAwLCJ0eSI6MX1dLCJvcCI6MjA' +
      'wLCJ2IjoiNC42LjkiLCJ3Ijo4MDB9';

  /**
   * A dataURL of an image for how a frame of the above |SAMPLE_LOTTIE_GREEN|
   * animation looks like.
   * @type {string}
   */
  const EXPECTED_FRAME_GREEN =
      'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDABALDA4MChAODQ4' +
      'SERATGCgaGBYWGDEjJR0oOjM9PDkzODdASFxOQERXRTc4UG1RV19iZ2hnPk1xeXBkeFxlZ' +
      '2P/2wBDARESEhgVGC8aGi9jQjhCY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2N' +
      'jY2NjY2NjY2NjY2NjY2NjY2P/wAARCADIASwDASIAAhEBAxEB/8QAGAABAQEBAQAAAAAAA' +
      'AAAAAAAAAMGBwX/xAAdEAEAAAYDAAAAAAAAAAAAAAAAAQIDBDRzBrHB/8QAGAEBAAMBAAA' +
      'AAAAAAAAAAAAAAAIEBgX/xAAbEQEAAgIDAAAAAAAAAAAAAAAAAQIyMwQFgf/aAAwDAQACE' +
      'QMRAD8A5+ADotlhW+uXpZGywrfXL0szNspY++UgCKIAAAAAAAAAAAAAAAAAAAAAAAAAAAA' +
      'yfL82hr9i1jJ8vzaGv2K5wt0L3Xb49eAA7jSAAOi2WFb65elkbLCt9cvSzM2ylj75SAIog' +
      'AAAAAAAAAAAAAAAAAAAAAAAAAAADJ8vzaGv2LWMny/Noa/YrnC3Qvddvj14ADuNIAA6LZY' +
      'Vvrl6WRssK31y9LMzbKWPvlIAiiAAAAAAAAAAAAAAAAAAAAAAAAAAAAMny/Noa/YtYyfL8' +
      '2hr9iucLdC912+PXgAO40gADotlhW+uXpZGywrfXL0szNspY++UgCKIAAAAAAAAAAAAAAA' +
      'AAAAAAAAAAAAAyfL82hr9i1jJ8vzaGv2K5wt0L3Xb49eAA7jSAAOi2WFb65elkbLCt9cvS' +
      'zM2ylj75SAIogAAAAAAAAAAAAAAAAAAAAAAAAAAADJ8vzaGv2LWMny/Noa/YrnC3Qvddvj' +
      '14ADuNIAA6LZYVvrl6WRssK31y9LMzbKWPvlIAiiAAAAAAAAAAAAAAAAAAAAAAAAAAAAMn' +
      'y/Noa/YtYyfL82hr9iucLdC912+PXgAO40gADotlhW+uXpZGywrfXL0szNspY++UgCKIAA' +
      'AAAAAAAAAAAAAAAAAAAAAAAAAAyfL82hr9i1jJ8vzaGv2K5wt0L3Xb49eAA7jSAAOi2WFb' +
      '65elkbLCt9cvSzM2ylj75SAIogAAAAAAAAAAAAAAAAAAAAAAAAAAADJ8vzaGv2LWMny/No' +
      'a/YrnC3Qvddvj14ADuNIAA6LZYVvrl6WRssK31y9LMzbKWPvlIAiiAAAAAAAAAAAAAAAAA' +
      'AAAAAAAAAAAMny/Noa/YtYyfL82hr9iucLdC912+PXgAO40gADotlhW+uXpZGywrfXL0sz' +
      'NspY++UgCKIAAAAAAAAAAAAAAAAAAAAAAAAAAAAyfL82hr9i1jJ8vzaGv2K5wt0L3Xb49e' +
      'AA7jSAAOi2WFb65elkbLCt9cvSzM2ylj75SAIogAAAAAAAAAAAAAAAAAAAAAAAAAAADJ8v' +
      'zaGv2LWMny/Noa/YrnC3Qvddvj14ADuNIAA6LZYVvrl6WRssK31y9LMzbKWPvlIAiiAAAA' +
      'AAAAAAAAAAAAAAAAAAAAAAAAMny/Noa/YtYyfL82hr9iucLdC912+PXgAO40gADotlhW+u' +
      'XpYGZtlLH3ykARRAAAAAAAAAAAAAAAAAAAAAAAAAAAAGT5fm0NfsQXOFuhe67fHrwAHcaR' +
      '//Z';

  /**
   * A dataURL of an image for how a frame of the above |SAMPLE_LOTTIE_BLUE|
   * animation looks like.
   * @type {string}
   */
  const EXPECTED_FRAME_BLUE =
      'data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDABALDA4MChAODQ4' +
      'SERATGCgaGBYWGDEjJR0oOjM9PDkzODdASFxOQERXRTc4UG1RV19iZ2hnPk1xeXBkeFxlZ' +
      '2P/2wBDARESEhgVGC8aGi9jQjhCY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2NjY2N' +
      'jY2NjY2NjY2NjY2NjY2NjY2P/wAARCADIASwDASIAAhEBAxEB/8QAGAABAQEBAQAAAAAAA' +
      'AAAAAAAAAQBAwf/xAAZEAEAAwEBAAAAAAAAAAAAAAAAATJxAgT/xAAaAQEAAgMBAAAAAAA' +
      'AAAAAAAAAAgYBAwQF/8QAGxEBAAICAwAAAAAAAAAAAAAAAAECBTEyNIH/2gAMAwEAAhEDE' +
      'QA/APPwAW8UjGs4pGNXmnGHNIAmwAAAAAAAAAAAAAAAAAAAAAAAAAAAAJ/TeMUJ/TeMebl' +
      'OtPidNuICqt4AC3ikY1nFIxq804w5pAE2AAAAAAAAAAAAAAAAAAAAAAAAAAAABP6bxihP6' +
      'bxjzcp1p8TptxAVVvAAW8UjGs4pGNXmnGHNIAmwAAAAAAAAAAAAAAAAAAAAAAAAAAAAJ/T' +
      'eMUJ/TeMeblOtPidNuICqt4AC3ikY1nFIxq804w5pAE2AAAAAAAAAAAAAAAAAAAAAAAAAA' +
      'AABP6bxihP6bxjzcp1p8TptxAVVvAAW8UjGs4pGNXmnGHNIAmwAAAAAAAAAAAAAAAAAAAA' +
      'AAAAAAAAJ/TeMUJ/TeMeblOtPidNuICqt4AC3ikY1nFIxq804w5pAE2AAAAAAAAAAAAAAA' +
      'AAAAAAAAAAAAABP6bxihP6bxjzcp1p8TptxAVVvAAW8UjGs4pGNXmnGHNIAmwAAAAAAAAA' +
      'AAAAAAAAAAAAAAAAAAAJ/TeMUJ/TeMeblOtPidNuICqt4AC3ikY1nFIxq804w5pAE2AAAA' +
      'AAAAAAAAAAAAAAAAAAAAAAAABP6bxihP6bxjzcp1p8TptxAVVvAAW8UjGs4pGNXmnGHNIA' +
      'mwAAAAAAAAAAAAAAAAAAAAAAAAAAAAJ/TeMUJ/TeMeblOtPidNuICqt4AC3ikY1nFIxq80' +
      '4w5pAE2AAAAAAAAAAAAAAAAAAAAAAAAAAAABP6bxihP6bxjzcp1p8TptxAVVvAAW8UjGs4' +
      'pGNXmnGHNIAmwAAAAAAAAAAAAAAAAAAAAAAAAAAAAJ/TeMUJ/TeMeblOtPidNuICqt4AC3' +
      'ikY1nFIxq804w5pAE2AAAAAAAAAAAAAAAAAAAAAAAAAAAABP6bxihP6bxjzcp1p8TptxAV' +
      'VvAAW8UjGgvNOMOaQBNgAAAAAAAAAAAAAAAAAAAAAAAAAAAAT+m8YDzcp1p8TptxAVVvf/' +
      '9k=';

  /** @type {?MockController} */
  let mockController;

  /** @type {!CrLottieElement} */
  let crLottieElement;

  /** @type {!HTMLDivElement} */
  let container;

  /** @type {?HTMLCanvasElement} */
  let canvas = null;

  /** @type {?blob} */
  let lottieWorkerJs = null;

  /** @type {Promise} */
  let waitForInitializeEvent;

  /** @type {Promise} */
  let waitForPlayingEvent;

  /** @type {Promise} */
  let waitForResizeEvent;

  setup(function(done) {
    mockController = new MockController();

    const xhr = new XMLHttpRequest();
    xhr.open('GET', LOTTIE_JS_URL, true);
    xhr.responseType = 'blob';
    xhr.send();
    xhr.onreadystatechange = function() {
      if (xhr.readyState === 4) {
        assertEquals(200, xhr.status);
        lottieWorkerJs = xhr.response;
        done();
      }
    };
  });

  teardown(function() {
    mockController.reset();
  });

  function createLottieElement() {
    PolymerTest.clearBody();
    crLottieElement = document.createElement('cr-lottie');
    crLottieElement.animationUrl = SAMPLE_LOTTIE_GREEN;
    crLottieElement.autoplay = true;

    waitForInitializeEvent =
        test_util.eventToPromise('cr-lottie-initialized', crLottieElement);
    waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    waitForResizeEvent =
        test_util.eventToPromise('cr-lottie-resized', crLottieElement);

    container = document.createElement('div');
    container.style.width = '300px';
    container.style.height = '200px';
    document.body.appendChild(container);
    container.appendChild(crLottieElement);

    canvas = crLottieElement.offscreenCanvas_;

    Polymer.dom.flush();
  }

  test('TestInitializeAnimationAndAutoPlay', async () => {
    createLottieElement();
    assertFalse(crLottieElement.isAnimationLoaded_);
    await waitForInitializeEvent;
    assertTrue(crLottieElement.isAnimationLoaded_);
    await waitForPlayingEvent;
  });

  // TODO(crbug.com/1021474): flaky.
  test.skip('TestResize', async () => {
    createLottieElement();
    await waitForInitializeEvent;
    await waitForPlayingEvent;
    await waitForResizeEvent;

    const newHeight = 300;
    const newWidth = 400;
    waitForResizeEvent =
        test_util.eventToPromise('cr-lottie-resized', crLottieElement)
            .then(function(e) {
              assertEquals(e.detail.height, newHeight);
              assertEquals(e.detail.width, newWidth);
            });

    // Update size of parent div container to see if the canvas is resized.
    container.style.width = newWidth + 'px';
    container.style.height = newHeight + 'px';
    await waitForResizeEvent;
  });

  test('TestPlayPause', async () => {
    createLottieElement();
    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForPauseEvent =
        test_util.eventToPromise('cr-lottie-paused', crLottieElement);
    crLottieElement.setPlay(false);
    await waitForPauseEvent;

    waitForPlayingEvent =
        test_util.eventToPromise('cr-lottie-playing', crLottieElement);
    crLottieElement.setPlay(true);
    await waitForPlayingEvent;
  });

  test('TestPlayBeforeInit', async () => {
    createLottieElement();
    assertTrue(crLottieElement.autoplay);

    crLottieElement.setPlay(false);
    assertFalse(crLottieElement.autoplay);

    crLottieElement.setPlay(true);
    assertTrue(crLottieElement.autoplay);

    await waitForInitializeEvent;
    await waitForPlayingEvent;
  });

  // TODO(crbug.com/1021474): flaky.
  test.skip('TestRenderFrame', async () => {
    createLottieElement();
    // Offscreen canvas has a race issue when used in this test framework. To
    // ensure that we capture a frame from the animation and not an empty frame,
    // we delay the capture by 2 seconds.
    // Note: This issue is only observed in tests.
    const kRaceTimeout = 2000;

    await waitForInitializeEvent;
    await waitForPlayingEvent;

    const waitForFrameRender = new Promise(function(resolve) {
                                 setTimeout(resolve, kRaceTimeout);
                               }).then(function() {
      const actualFrame =
          crLottieElement.canvasElement_.toDataURL('image/jpeg', 0.5);
      assertEquals(actualFrame, EXPECTED_FRAME_GREEN);
    });

    await waitForFrameRender;
  });

  test('TestHidden', async () => {
    await waitForPlayingEvent;

    assertFalse(crLottieElement.$$('canvas').hidden);
    crLottieElement.hidden = true;
    assertTrue(crLottieElement.$$('canvas').hidden);
  });

  test('TestDetachBeforeImageLoaded', async () => {
    const mockXhr = {};
    mockXhr.open = mockController.createFunctionMock(mockXhr, 'open');
    mockXhr.send = mockController.createFunctionMock(mockXhr, 'send');
    mockXhr.abort = mockController.createFunctionMock(mockXhr, 'abort');

    const mockXhrConstructor =
        mockController.createFunctionMock(window, 'XMLHttpRequest');

    // Expectations for loading the worker.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation(
        'GET', 'chrome://resources/lottie/lottie_worker.min.js', true);
    mockXhr.send.addExpectation();

    // Expectations for loading the image and aborting it.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation('GET', SAMPLE_LOTTIE_GREEN, true);
    mockXhr.send.addExpectation();
    mockXhr.abort.addExpectation();

    mockXhrConstructor.returnValue = mockXhr;

    createLottieElement();

    // Return the lottie worker.
    mockXhr.response = lottieWorkerJs;
    mockXhr.readyState = 4;
    mockXhr.status = 200;
    mockXhr.onreadystatechange();

    // Detaching the element before the image has loaded should abort the
    // request.
    crLottieElement.remove();
    mockController.verifyMocks();
  });

  test('TestLoadNewImageWhileOldImageIsStillLoading', async () => {
    const mockXhr = {};
    mockXhr.open = mockController.createFunctionMock(mockXhr, 'open');
    mockXhr.send = mockController.createFunctionMock(mockXhr, 'send');
    mockXhr.abort = mockController.createFunctionMock(mockXhr, 'abort');

    const mockXhrConstructor =
        mockController.createFunctionMock(window, 'XMLHttpRequest');

    // Expectations for loading the worker.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation(
        'GET', 'chrome://resources/lottie/lottie_worker.min.js', true);
    mockXhr.send.addExpectation();

    // Expectations for loading the first image and aborting it.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation('GET', SAMPLE_LOTTIE_GREEN, true);
    mockXhr.send.addExpectation();
    mockXhr.abort.addExpectation();

    // Expectations for loading the second image.
    mockXhrConstructor.addExpectation();
    mockXhr.open.addExpectation('GET', SAMPLE_LOTTIE_BLUE, true);
    mockXhr.send.addExpectation();

    mockXhrConstructor.returnValue = mockXhr;

    createLottieElement();

    // Return the lottie worker.
    mockXhr.response = lottieWorkerJs;
    mockXhr.readyState = 4;
    mockXhr.status = 200;
    mockXhr.onreadystatechange();

    // Attempting to load a new image should abort the first request and start a
    // new one.
    crLottieElement.animationUrl = SAMPLE_LOTTIE_BLUE;

    mockController.verifyMocks();
  });
});
