// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/customize_dialog.js';

import {BrowserProxy} from 'chrome://new-tab-page/browser_proxy.js';
import {createTestProxy} from 'chrome://test/new_tab_page/test_support.js';
import {flushTasks, isVisible, waitAfterNextRender} from 'chrome://test/test_util.m.js';

suite('NewTabPageCustomizeDialogTest', () => {
  /** @type {!CustomizeDialogElement} */
  let customizeDialog;

  setup(() => {
    PolymerTest.clearBody();

    const testProxy = createTestProxy();
    testProxy.handler.setResultFor('getBackgroundCollections', Promise.resolve({
      collections: [],
    }));
    testProxy.handler.setResultFor('getChromeThemes', Promise.resolve({
      chromeThemes: [],
    }));
    BrowserProxy.instance_ = testProxy;

    customizeDialog = document.createElement('ntp-customize-dialog');
    document.body.appendChild(customizeDialog);
    return flushTasks();
  });

  test('creating customize dialog opens cr dialog', () => {
    // Assert.
    assertTrue(customizeDialog.$.dialog.open);
  });

  test('background page selected at start', () => {
    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(shownPages[0].getAttribute('page-name'), 'backgrounds');
  });

  test('selecting menu item shows page', async () => {
    // Act.
    customizeDialog.$.menu.querySelector('[page-name=themes]').click();
    await flushTasks();

    // Assert.
    const shownPages =
        customizeDialog.shadowRoot.querySelectorAll('#pages .iron-selected');
    assertEquals(shownPages.length, 1);
    assertEquals(shownPages[0].getAttribute('page-name'), 'themes');
  });

  suite('scroll borders', () => {
    /**
     * @param {!HTMLElement} container
     * @private
     */
    async function testScrollBorders(container) {
      const assertHidden = el => {
        assertTrue(el.matches('[scroll-border]:not([show])'));
      };
      const assertShown = el => {
        assertTrue(el.matches('[scroll-border][show]'));
      };
      const {firstElementChild: top, lastElementChild: bottom} = container;
      const scrollableElement = top.nextSibling;
      const dialogBody =
          customizeDialog.shadowRoot.querySelector('div[slot=body]');
      const heightWithBorders = `${scrollableElement.scrollHeight + 2}px`;
      dialogBody.style.height = heightWithBorders;
      assertHidden(top);
      assertHidden(bottom);
      dialogBody.style.height = '50px';
      await waitAfterNextRender();
      assertHidden(top);
      assertShown(bottom);
      scrollableElement.scrollTop = 1;
      await waitAfterNextRender();
      assertShown(top);
      assertShown(bottom);
      scrollableElement.scrollTop = scrollableElement.scrollHeight;
      await waitAfterNextRender();
      assertShown(top);
      assertHidden(bottom);
      dialogBody.style.height = heightWithBorders;
      await waitAfterNextRender();
      assertHidden(top);
      assertHidden(bottom);
    }

    test('menu', () => testScrollBorders(customizeDialog.$.menuContainer));
    test('pages', () => testScrollBorders(customizeDialog.$.pagesContainer));
  });
});
