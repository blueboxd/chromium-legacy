// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BrowserProxy, hexColorToSkColor} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertNotStyle, assertStyle, createTestProxy, keydown} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

/**
 * @param {!Element} element
 * @param {!Element} reference
 * @return {!{top: number, right: number, bottom: number, left: number}}
 */
function getRelativePosition(element, reference) {
  const referenceRect = reference.getBoundingClientRect();
  const elementRect = element.getBoundingClientRect();
  return {
    top: elementRect.top - referenceRect.top,
    right: elementRect.right - referenceRect.right,
    bottom: elementRect.bottom - referenceRect.bottom,
    left: elementRect.left - referenceRect.left,
  };
}

/**
 * @param {number} width
 * @param {number} height
 * @return {string}
 */
function createImageDataUrl(width, height) {
  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  return canvas.toDataURL('image/png');
}

/**
 * @param {number} width
 * @param {number} height
 * @return {!newTabPage.mojom.Doodle}
 */
function createImageDoodle(width, height) {
  width = width || 500;
  height = height || 200;
  return {
    content: {
      imageDoodle: {
        imageUrl: {url: createImageDataUrl(width, height)},
        onClickUrl: {url: 'https://foo.com'},
        shareButton: {
          backgroundColor: {value: 0xFFFF0000},
          x: 0,
          y: 0,
          iconUrl: {url: 'data:bar'},
        },
        imageImpressionLogUrl: {url: 'https://log.com'},
        width,
        height,
        backgroundColor: {value: 0xffffffff},
      }
    }
  };
}

function createSuite(themeModeDoodlesEnabled) {
  /**
   * @implements {BrowserProxy}
   * @extends {TestBrowserProxy}
   */
  let testProxy;

  async function createLogo(doodle = null) {
    testProxy.handler.setResultFor('getDoodle', Promise.resolve({
      doodle: doodle,
    }));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);
    logo.backgroundColor = {value: 0xffffffff};
    await flushTasks();
    return logo;
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({themeModeDoodlesEnabled});
  });

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    testProxy.handler.setResultFor('onDoodleImageRendered', Promise.resolve({
      imageClickParams: '',
      interactionLogUrl: null,
      shareId: '',
    }));
    BrowserProxy.instance_ = testProxy;
  });

  test('setting simple doodle shows image', async () => {
    // Arrange.
    const doodle = createImageDoodle(/*width=*/ 500, /*height=*/ 200);
    doodle.content.imageDoodle.shareButton = {
      backgroundColor: {value: 0xFFFF0000},
      x: 10,
      y: 20,
      iconUrl: {url: 'data:bar'},
    };

    // Act.
    const logo = await createLogo(doodle);

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals(
        $$(logo, '#image').src, doodle.content.imageDoodle.imageUrl.url);
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertEquals(500, $$(logo, '#image').offsetWidth);
    assertEquals(200, $$(logo, '#image').offsetHeight);
    assertNotStyle($$(logo, '#shareButton'), 'display', 'none');
    assertStyle($$(logo, '#shareButton'), 'background-color', 'rgb(255, 0, 0)');
    const shareButtonPosition =
        getRelativePosition($$(logo, '#shareButton'), $$(logo, '#image'));
    assertEquals(10, shareButtonPosition.left);
    assertEquals(20, shareButtonPosition.top);
    assertEquals(26, $$(logo, '#shareButton').offsetWidth);
    assertEquals(26, $$(logo, '#shareButton').offsetHeight);
    assertEquals($$(logo, '#shareButtonImage').src, 'data:bar');
    assertStyle($$(logo, '#animation'), 'display', 'none');
    assertFalse(!!$$(logo, '#iframe'));
  });

  [null, '#ff0000'].forEach(color => {
    test(`${color || 'no'} background color shows box`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      doodle.content.imageDoodle.backgroundColor.value = 0xff0000ff;

      // Act.
      const logo = await createLogo(doodle);
      logo.backgroundColor = !color || hexColorToSkColor(color);

      // Assert.
      assertStyle($$(logo, '#imageDoodle'), 'padding', '16px 24px');
      assertStyle(
          $$(logo, '#imageDoodle'), 'background-color', 'rgb(0, 0, 255)');
    });
  });

  test('setting too large image doodle resizes image', async () => {
    // Arrange.
    const doodle = createImageDoodle(/*width=*/ 1000, /*height=*/ 500);
    doodle.content.imageDoodle.shareButton.x = 10;
    doodle.content.imageDoodle.shareButton.y = 20;

    // Act.
    const logo = await createLogo(doodle);

    // Assert.
    assertEquals(460, $$(logo, '#image').offsetWidth);
    assertEquals(230, $$(logo, '#image').offsetHeight);
    const shareButtonPosition =
        getRelativePosition($$(logo, '#shareButton'), $$(logo, '#image'));
    assertEquals(5, Math.round(shareButtonPosition.left));
    assertEquals(9, Math.round(shareButtonPosition.top));
    assertEquals(12, $$(logo, '#shareButton').offsetWidth);
    assertEquals(12, $$(logo, '#shareButton').offsetHeight);
  });

  test('setting animated doodle shows image', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.content.imageDoodle.imageUrl = {url: 'data:foo'};
    doodle.content.imageDoodle.animationUrl = {url: 'https://foo.com'};

    // Act.
    const logo = await createLogo(doodle);

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#image').src, 'data:foo');
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertStyle($$(logo, '#animation'), 'display', 'none');
    assertFalse(!!$$(logo, '#iframe'));
  });

  test('setting interactive doodle shows iframe', async () => {
    // Act.
    const logo = await createLogo({
      content: {
        interactiveDoodle: {
          url: {url: 'https://foo.com'},
          width: 200,
          height: 100,
        }
      }
    });
    logo.dark = false;

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertNotStyle($$(logo, '#iframe'), 'display', 'none');
    assertStyle($$(logo, '#iframe'), 'width', '200px');
    assertStyle($$(logo, '#iframe'), 'height', '100px');
    assertStyle($$(logo, '#imageDoodle'), 'display', 'none');
    if (themeModeDoodlesEnabled) {
      assertEquals(
          $$(logo, '#iframe').src, 'https://foo.com/?theme_messages=0');
      assertEquals(1, testProxy.getCallCount('postMessage'));
      const [iframe, {cmd, dark}, origin] =
          await testProxy.whenCalled('postMessage');
      assertEquals($$(logo, '#iframe'), iframe);
      assertEquals('changeMode', cmd);
      assertEquals(false, dark);
      assertEquals('https://foo.com', origin);
    } else {
      assertEquals($$(logo, '#iframe').src, 'https://foo.com/');
    }
  });

  test('message only after mode has been set', async () => {
    // Act (no mode).
    const logo = await createLogo({
      content: {
        interactiveDoodle: {
          url: {url: 'https://foo.com'},
          width: 200,
          height: 100,
        }
      }
    });

    // Assert (no mode).
    assertEquals(0, testProxy.getCallCount('postMessage'));

    // Act (setting mode).
    logo.dark = true;

    // Assert (setting mode).
    if (themeModeDoodlesEnabled) {
      assertEquals(1, testProxy.getCallCount('postMessage'));
      const [iframe, {cmd, dark}, origin] =
          await testProxy.whenCalled('postMessage');
      assertEquals($$(logo, '#iframe'), iframe);
      assertEquals('changeMode', cmd);
      assertEquals(true, dark);
      assertEquals('https://foo.com', origin);
    } else {
      assertEquals(0, testProxy.getCallCount('postMessage'));
    }
  });

  test('disallowing doodle shows logo', async () => {
    // Act.
    const logo = await createLogo(createImageDoodle());
    logo.doodleAllowed = false;
    Array.from(logo.shadowRoot.querySelectorAll('dom-if')).forEach((domIf) => {
      domIf.render();
    });

    // Assert.
    assertNotStyle($$(logo, '#logo'), 'display', 'none');
    assertEquals($$(logo, '#doodle'), null);
  });

  test('before doodle loaded shows nothing', () => {
    // Act.
    testProxy.handler.setResultFor('getDoodle', new Promise(() => {}));
    const logo = document.createElement('ntp-logo');
    document.body.appendChild(logo);

    // Assert.
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#doodle'), null);
  });

  test('unavailable doodle shows logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle($$(logo, '#logo'), 'display', 'none');
    assertEquals($$(logo, '#doodle'), null);
  });

  test('not setting-single colored shows multi-colored logo', async () => {
    // Act.
    const logo = await createLogo();

    // Assert.
    assertNotStyle($$(logo, '#multiColoredLogo'), 'display', 'none');
    assertStyle($$(logo, '#singleColoredLogo'), 'display', 'none');
  });

  test('setting single-colored shows single-colored logo', async () => {
    // Act.
    const logo = await createLogo();
    logo.singleColored = true;
    logo.style.setProperty('--ntp-logo-color', 'red');

    // Assert.
    assertNotStyle($$(logo, '#singleColoredLogo'), 'display', 'none');
    assertStyle(
        $$(logo, '#singleColoredLogo'), 'background-color', 'rgb(255, 0, 0)');
    assertStyle($$(logo, '#multiColoredLogo'), 'display', 'none');
  });

  // Disabled for flakiness, see https://crbug.com/1065812.
  test.skip('receiving resize message resizes doodle', async () => {
    // Arrange.
    const logo = await createLogo(
        {content: {interactiveDoodle: {url: {url: 'https://foo.com'}}}});
    const transitionend = eventToPromise('transitionend', $$(logo, '#iframe'));

    // Act.
    window.postMessage(
        {
          cmd: 'resizeDoodle',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await transitionend;

    // Assert.
    const transitionedProperties = window.getComputedStyle($$(logo, '#iframe'))
                                       .getPropertyValue('transition-property')
                                       .trim()
                                       .split(',')
                                       .map(s => s.trim());
    assertStyle($$(logo, '#iframe'), 'transition-duration', '0.5s');
    assertTrue(transitionedProperties.includes('height'));
    assertTrue(transitionedProperties.includes('width'));
    assertEquals($$(logo, '#iframe').offsetHeight, 500);
    assertEquals($$(logo, '#iframe').offsetWidth, 700);
    assertGE(logo.offsetHeight, 500);
    assertGE(logo.offsetWidth, 700);
  });

  test('receiving other message does not resize doodle', async () => {
    // Arrange.
    const logo = await createLogo(
        {content: {interactiveDoodle: {url: {url: 'https://foo.com'}}}});
    const height = $$(logo, '#iframe').offsetHeight;
    const width = $$(logo, '#iframe').offsetWidth;

    // Act.
    window.postMessage(
        {
          cmd: 'foo',
          duration: '500ms',
          height: '500px',
          width: '700px',
        },
        '*');
    await flushTasks();

    // Assert.
    assertEquals($$(logo, '#iframe').offsetHeight, height);
    assertEquals($$(logo, '#iframe').offsetWidth, width);
  });

  test('receiving mode message sends mode', async () => {
    // Arrange.
    const logo = await createLogo(
        {content: {interactiveDoodle: {url: {url: 'https://foo.com'}}}});
    logo.dark = false;
    testProxy.resetResolver('postMessage');

    // Act.
    window.postMessage({cmd: 'sendMode'}, '*');
    await flushTasks();

    // Assert.
    if (themeModeDoodlesEnabled) {
      assertEquals(1, testProxy.getCallCount('postMessage'));
      const [_, {cmd, dark}, origin] =
          await testProxy.whenCalled('postMessage');
      assertEquals('changeMode', cmd);
      assertEquals(false, dark);
      assertEquals('https://foo.com', origin);
    } else {
      assertEquals(0, testProxy.getCallCount('postMessage'));
    }
  });

  test('clicking simple doodle opens link', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.content.imageDoodle.onClickUrl = {url: 'https://foo.com'};
    const logo = await createLogo(doodle);

    // Act.
    $$(logo, '#image').click();
    const url = await testProxy.whenCalled('open');

    // Assert.
    assertEquals(url, 'https://foo.com/');
  });

  [' ', 'Enter'].forEach(key => {
    test(`pressing ${key} on simple doodle opens link`, async () => {
      // Arrange.
      const doodle = createImageDoodle();
      doodle.content.imageDoodle.onClickUrl = {url: 'https://foo.com'};
      const logo = await createLogo(doodle);

      // Act.
      keydown($$(logo, '#image'), key);
      const url = await testProxy.whenCalled('open');

      // Assert.
      assertEquals(url, 'https://foo.com/');
    });
  });

  test('clicking image of animated doodle starts animation', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.content.imageDoodle.animationUrl = {url: 'https://foo.com'};
    const logo = await createLogo(doodle);

    // Act.
    $$(logo, '#image').click();

    // Assert.
    assertEquals(testProxy.getCallCount('open'), 0);
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertNotStyle($$(logo, '#animation'), 'display', 'none');
    assertEquals($$(logo, '#animation').path, 'image?https://foo.com');
    assertDeepEquals(
        $$(logo, '#image').getBoundingClientRect(),
        $$(logo, '#animation').getBoundingClientRect());
  });

  test('clicking animation of animated doodle opens link', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.content.imageDoodle.animationUrl = {url: 'https://foo.com'};
    doodle.content.imageDoodle.onClickUrl = {url: 'https://bar.com'};
    const logo = await createLogo(doodle);
    $$(logo, '#image').click();

    // Act.
    $$(logo, '#animation').click();
    const url = await testProxy.whenCalled('open');

    // Assert.
    assertEquals(url, 'https://bar.com/');
  });

  test('share dialog removed on start', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());

    // Assert.
    assertFalse(!!logo.shadowRoot.querySelector('ntp-doodle-share-dialog'));
  });

  test('clicking share button adds share dialog', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());

    // Act.
    $$(logo, '#shareButton').click();
    await flushTasks();

    // Assert.
    assertTrue(!!logo.shadowRoot.querySelector('ntp-doodle-share-dialog'));
  });

  test('closing share dialog removes share dialog', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle());
    $$(logo, '#shareButton').click();
    await flushTasks();

    // Act.
    logo.shadowRoot.querySelector('ntp-doodle-share-dialog')
        .dispatchEvent(new Event('close'));
    await flushTasks();

    // Assert.
    assertFalse(!!logo.shadowRoot.querySelector('ntp-doodle-share-dialog'));
  });

  test('simple doodle logging flow', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.content.imageDoodle.onClickUrl = {url: 'https://click.com?ct=supi'};
    doodle.content.imageDoodle.imageImpressionLogUrl = {url: 'https://log.com'};
    const logo = await createLogo(doodle);
    testProxy.handler.setResultFor('onDoodleImageRendered', Promise.resolve({
      imageClickParams: 'foo=bar&hello=world',
      interactionLogUrl: null,
      shareId: '123',
    }));

    // Act (load).
    $$(logo, '#image').dispatchEvent(new Event('load'));

    // Assert (load).
    const [type, _, logUrl] =
        await testProxy.handler.whenCalled('onDoodleImageRendered');
    assertEquals(newTabPage.mojom.DoodleImageType.STATIC, type);
    assertEquals('https://log.com', logUrl.url);

    // Act (click).
    $$(logo, '#image').click();

    // Assert (click).
    const [type2] = await testProxy.handler.whenCalled('onDoodleImageClicked');
    const onClickUrl = await testProxy.whenCalled('open');
    assertEquals(newTabPage.mojom.DoodleImageType.STATIC, type2);
    assertEquals('https://click.com/?ct=supi&foo=bar&hello=world', onClickUrl);

    // Act (share).
    $$(logo, '#shareButton').click();
    await flushTasks();
    $$(logo, 'ntp-doodle-share-dialog').dispatchEvent(new CustomEvent('share', {
      detail: newTabPage.mojom.DoodleShareChannel.FACEBOOK
    }));

    // Assert (share).
    const [channel, doodleId, shareId] =
        await testProxy.handler.whenCalled('onDoodleShared');
    assertEquals(newTabPage.mojom.DoodleShareChannel.FACEBOOK, channel);
    assertEquals('supi', doodleId);
    assertEquals('123', shareId);
  });

  test('animated doodle logging flow', async () => {
    // Arrange.
    const doodle = createImageDoodle();
    doodle.content.imageDoodle.onClickUrl = {url: 'https://click.com?ct=supi'};
    doodle.content.imageDoodle.imageImpressionLogUrl = {url: 'https://log.com'};
    doodle.content.imageDoodle.animationUrl = {url: 'https://animation.com'};
    doodle.content.imageDoodle.animationImpressionLogUrl = {
      url: 'https://animation_log.com'
    };
    const logo = await createLogo(doodle);
    testProxy.handler.setResultFor('onDoodleImageRendered', Promise.resolve({
      imageClickParams: '',
      interactionLogUrl: {url: 'https://interaction.com'},
      shareId: '',
    }));

    // Act (CTA load).
    $$(logo, '#image').dispatchEvent(new Event('load'));

    // Assert (CTA load).
    const [type, _, logUrl] =
        await testProxy.handler.whenCalled('onDoodleImageRendered');
    assertEquals(newTabPage.mojom.DoodleImageType.CTA, type);
    assertEquals('https://log.com', logUrl.url);

    // Act (CTA click).
    testProxy.handler.resetResolver('onDoodleImageRendered');
    testProxy.handler.setResultFor('onDoodleImageRendered', Promise.resolve({
      imageClickParams: 'foo=bar&hello=world',
      interactionLogUrl: null,
      shareId: '123',
    }));
    $$(logo, '#image').click();

    // Assert (CTA click).
    const [type2, interactionLogUrl] =
        await testProxy.handler.whenCalled('onDoodleImageClicked');
    assertEquals(newTabPage.mojom.DoodleImageType.CTA, type2);
    assertEquals('https://interaction.com', interactionLogUrl.url);

    // Assert (animation load). Also triggered by clicking #image.
    const [type3, __, logUrl2] =
        await testProxy.handler.whenCalled('onDoodleImageRendered');
    assertEquals(newTabPage.mojom.DoodleImageType.ANIMATION, type3);
    assertEquals('https://animation_log.com', logUrl2.url);

    // Act (animation click).
    testProxy.handler.resetResolver('onDoodleImageClicked');
    $$(logo, '#animation').click();

    // Assert (animation click).
    const [type4, ___] =
        await testProxy.handler.whenCalled('onDoodleImageClicked');
    const onClickUrl = await testProxy.whenCalled('open');
    assertEquals(newTabPage.mojom.DoodleImageType.ANIMATION, type4);
    assertEquals('https://click.com/?ct=supi&foo=bar&hello=world', onClickUrl);

    // Act (share).
    $$(logo, '#shareButton').click();
    await flushTasks();
    $$(logo, 'ntp-doodle-share-dialog').dispatchEvent(new CustomEvent('share', {
      detail: newTabPage.mojom.DoodleShareChannel.TWITTER
    }));

    // Assert (share).
    const [channel, doodleId, shareId] =
        await testProxy.handler.whenCalled('onDoodleShared');
    assertEquals(newTabPage.mojom.DoodleShareChannel.TWITTER, channel);
    assertEquals('supi', doodleId);
    assertEquals('123', shareId);
  });
}

suite('NewTabPageLogoTest', () => {
  [true, false].forEach(themeModeDoodlesEnabled => {
    const enabled = themeModeDoodlesEnabled ? 'enabled' : 'disabled';
    suite(`theme mode doodles ${enabled}`, () => {
      createSuite(themeModeDoodlesEnabled);
    });
  });
});
