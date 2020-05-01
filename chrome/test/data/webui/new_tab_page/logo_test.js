// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$$, BrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {assertNotStyle, assertStyle, createTestProxy, keydown} from 'chrome://test/new_tab_page/test_support.js';
import {eventToPromise, flushTasks} from 'chrome://test/test_util.m.js';

function createImageDoodle(config = {}) {
  const doodle = {
    content: {
      imageDoodle: {
        imageUrl: {url: config.imageUrl || 'data:foo'},
        onClickUrl: {url: config.onClickUrl || 'https://foo.com'},
        shareButton: {
          backgroundColor: {value: config.backgroundColor || 0xFFFF0000},
          x: config.x || 0,
          y: config.y || 0,
          iconUrl: {url: config.iconUrl || 'data:bar'},
        },
      }
    }
  };
  if (config.animationUrl) {
    doodle.content.imageDoodle.animationUrl = {url: config.animationUrl};
  }
  return doodle;
}

suite('NewTabPageLogoTest', () => {
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
    await flushTasks();
    return logo;
  }

  setup(() => {
    PolymerTest.clearBody();

    testProxy = createTestProxy();
    BrowserProxy.instance_ = testProxy;
  });

  test('setting simple doodle shows image', async () => {
    // Act.
    const logo = await createLogo(createImageDoodle({
      imageUrl: 'data:foo',
      backgroundColor: 0xFFFF0000,
      x: 11,
      y: 12,
      iconUrl: 'data:bar',
    }));

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#image').src, 'data:foo');
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertNotStyle($$(logo, '#shareButton'), 'display', 'none');
    assertStyle($$(logo, '#shareButton'), 'background-color', 'rgb(255, 0, 0)');
    assertStyle($$(logo, '#shareButton'), 'left', '11px');
    assertStyle($$(logo, '#shareButton'), 'top', '12px');
    assertEquals($$(logo, '#shareButtonImage').src, 'data:bar');
    assertStyle($$(logo, '#animation'), 'display', 'none');
    assertStyle($$(logo, '#iframe'), 'display', 'none');
  });

  test('setting animated doodle shows image', async () => {
    // Act.
    const logo = await createLogo(createImageDoodle({
      imageUrl: 'data:foo',
      animationUrl: 'https://foo.com',
    }));

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#image').src, 'data:foo');
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertStyle($$(logo, '#animation'), 'display', 'none');
    assertStyle($$(logo, '#iframe'), 'display', 'none');
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

    // Assert.
    assertNotStyle($$(logo, '#doodle'), 'display', 'none');
    assertEquals($$(logo, '#logo'), null);
    assertEquals($$(logo, '#iframe').path, 'iframe?https://foo.com');
    assertNotStyle($$(logo, '#iframe'), 'display', 'none');
    assertStyle($$(logo, '#iframe'), 'width', '200px');
    assertStyle($$(logo, '#iframe'), 'height', '100px');
    assertStyle($$(logo, '#imageContainer'), 'display', 'none');
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

  test('clicking simple doodle opens link', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle({
      imageUrl: 'data:foo',
      onClickUrl: 'https://foo.com',
    }));

    // Act.
    $$(logo, '#image').click();
    const url = await testProxy.whenCalled('open');

    // Assert.
    assertEquals(url, 'https://foo.com');
  });

  [' ', 'Enter'].forEach(key => {
    test(`pressing ${key} on simple doodle opens link`, async () => {
      // Arrange.
      const logo = await createLogo({
        content: {
          imageDoodle: {
            imageUrl: {url: 'data:foo'},
            onClickUrl: {url: 'https://foo.com'},
          }
        }
      });

      // Act.
      keydown($$(logo, '#image'), key);
      const url = await testProxy.whenCalled('open');

      // Assert.
      assertEquals(url, 'https://foo.com');
    });
  });

  test('clicking image of animated doodle starts animation', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle({
      imageUrl: 'data:foo',
      animationUrl: 'https://foo.com',
    }));

    // Act.
    $$(logo, '#image').click();

    // Assert.
    assertEquals(testProxy.getCallCount('open'), 0);
    assertNotStyle($$(logo, '#image'), 'display', 'none');
    assertNotStyle($$(logo, '#animation'), 'display', 'none');
    assertEquals($$(logo, '#animation').path, 'image?https://foo.com');
  });

  test('clicking animation of animated doodle opens link', async () => {
    // Arrange.
    const logo = await createLogo(createImageDoodle({
      imageUrl: 'data:foo',
      animationUrl: 'https://foo.com',
      onClickUrl: 'https://bar.com',
    }));
    $$(logo, '#image').click();

    // Act.
    $$(logo, '#animation').click();
    const url = await testProxy.whenCalled('open');

    // Assert.
    assertEquals(url, 'https://bar.com');
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
});
