// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('manager tests', function() {
  /** @type {!downloads.Manager} */
  let manager;

  /** @type {!downloads.mojom.PageHandlerCallbackRouter} */
  let pageRouterProxy;

  /** @type {TestDownloadsProxy} */
  let testBrowserProxy;

  setup(function() {
    PolymerTest.clearBody();

    testBrowserProxy = new TestDownloadsProxy();
    callbackRouterRemote = testBrowserProxy.callbackRouterRemote;
    downloads.BrowserProxy.instance_ = testBrowserProxy;

    manager = document.createElement('downloads-manager');
    document.body.appendChild(manager);
    assertEquals(manager, downloads.Manager.get());
  });

  test('long URLs elide', async () => {
    callbackRouterRemote.insertItems(0, [createDownload({
                                       fileName: 'file name',
                                       state: downloads.States.COMPLETE,
                                       sinceString: 'Today',
                                       url: 'a'.repeat(1000),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();

    const item = manager.$$('downloads-item');
    assertLT(item.$$('#url').offsetWidth, item.offsetWidth);
    assertEquals(300, item.$$('#url').textContent.length);
  });

  test('inserting items at beginning render dates correctly', async () => {
    const countDates = () => {
      const items = manager.shadowRoot.querySelectorAll('downloads-item');
      return Array.from(items).reduce((soFar, item) => {
        return item.$$('div[id=date]:not(:empty)') ? soFar + 1 : soFar;
      }, 0);
    };

    const download1 = createDownload();
    const download2 = createDownload();

    callbackRouterRemote.insertItems(0, [download1, download2]);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();
    assertEquals(1, countDates());

    callbackRouterRemote.removeItem(0);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();
    assertEquals(1, countDates());

    callbackRouterRemote.insertItems(0, [download1]);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();
    assertEquals(1, countDates());
  });

  test('update', async () => {
    const dangerousDownload = createDownload({
      dangerType: downloads.DangerType.DANGEROUS_FILE,
      state: downloads.States.DANGEROUS,
    });
    callbackRouterRemote.insertItems(0, [dangerousDownload]);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();
    assertTrue(!!manager.$$('downloads-item').$$('.dangerous'));

    const safeDownload = Object.assign({}, dangerousDownload, {
      dangerType: downloads.DangerType.NOT_DANGEROUS,
      state: downloads.States.COMPLETE,
    });
    callbackRouterRemote.updateItem(0, safeDownload);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();
    assertFalse(!!manager.$$('downloads-item').$$('.dangerous'));
  });

  test('remove', async () => {
    callbackRouterRemote.insertItems(0, [createDownload({
                                       fileName: 'file name',
                                       state: downloads.States.COMPLETE,
                                       sinceString: 'Today',
                                       url: 'a'.repeat(1000),
                                     })]);
    await callbackRouterRemote.$.flushForTesting();
    Polymer.dom.flush();
    const item = manager.$$('downloads-item');

    item.$.remove.click();
    await testBrowserProxy.handler.whenCalled('remove');
    Polymer.dom.flush();
    const list = manager.$$('iron-list');
    assertTrue(list.hidden);
  });

  test('toolbar hasClearableDownloads set correctly', async () => {
    const clearable = createDownload();
    callbackRouterRemote.insertItems(0, [clearable]);
    const checkNotClearable = async state => {
      const download = createDownload({state: state});
      callbackRouterRemote.updateItem(0, clearable);
      await callbackRouterRemote.$.flushForTesting();
      assertTrue(manager.$.toolbar.hasClearableDownloads);
      callbackRouterRemote.updateItem(0, download);
      await callbackRouterRemote.$.flushForTesting();
      assertFalse(manager.$.toolbar.hasClearableDownloads);
    };
    await checkNotClearable(downloads.States.DANGEROUS);
    await checkNotClearable(downloads.States.IN_PROGRESS);
    await checkNotClearable(downloads.States.PAUSED);

    callbackRouterRemote.updateItem(0, clearable);
    callbackRouterRemote.insertItems(
        1, [createDownload({state: downloads.States.DANGEROUS})]);
    await callbackRouterRemote.$.flushForTesting();
    assertTrue(manager.$.toolbar.hasClearableDownloads);
    callbackRouterRemote.removeItem(0);
    await callbackRouterRemote.$.flushForTesting();
    assertFalse(manager.$.toolbar.hasClearableDownloads);
  });

  test('loadTimeData contains isManaged and managedByOrg', function() {
    // Check that loadTimeData contains these values.
    loadTimeData.getBoolean('isManaged');
    loadTimeData.getString('managedByOrg');
  });

  test('toast is shown when clear-all-command is fired', () => {
    const toastManager = cr.toastManager.getInstance();
    assertFalse(toastManager.isToastOpen);
    const event = new Event('command', {bubbles: true});
    event.command = {id: 'clear-all-command'};
    manager.dispatchEvent(event);
    assertTrue(toastManager.isToastOpen);
    assertFalse(toastManager.isUndoButtonHidden);
  });

  test('toast is hidden when undo-command is fired', () => {
    const toastManager = cr.toastManager.getInstance();
    toastManager.show('');
    const event = new Event('command', {bubbles: true});
    event.command = {id: 'undo-command'};
    assertTrue(toastManager.isToastOpen);
    manager.dispatchEvent(event);
    assertFalse(toastManager.isToastOpen);
  });

  test('toast is hidden when undo is clicked', () => {
    const toastManager = cr.toastManager.getInstance();
    toastManager.show('');
    assertTrue(toastManager.isToastOpen);
    toastManager.dispatchEvent(new Event('undo-click'));
    assertFalse(toastManager.isToastOpen);
  });
});
