// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {DragManager, PLACEHOLDER_TAB_ID} from 'chrome://tab-strip/drag_manager.js';
import {TabElement} from 'chrome://tab-strip/tab.js';
import {TabGroupElement} from 'chrome://tab-strip/tab_group.js';
import {TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

class MockDelegate extends HTMLElement {
  getIndexOfTab(tabElement) {
    return Array.from(this.querySelectorAll('tabstrip-tab'))
        .indexOf(tabElement);
  }

  placeTabElement(element, index, pinned, groupId) {
    element.remove();

    const parent =
        groupId ? this.querySelector(`[data-group-id=${groupId}]`) : this;
    parent.insertBefore(element, this.children[index]);
  }

  placeTabGroupElement(element, index) {
    element.remove();
    this.insertBefore(element, this.children[index]);
  }

  showDropPlaceholder(element) {
    this.appendChild(element);
  }
}
customElements.define('mock-delegate', MockDelegate);

class MockDataTransfer extends DataTransfer {
  constructor() {
    super();

    this.dragImageData = {
      image: undefined,
      offsetX: undefined,
      offsetY: undefined,
    };

    this.dropEffect_ = 'none';
    this.effectAllowed_ = 'none';
  }

  get dropEffect() {
    return this.dropEffect_;
  }

  set dropEffect(effect) {
    this.dropEffect_ = effect;
  }

  get effectAllowed() {
    return this.effectAllowed_;
  }

  set effectAllowed(effect) {
    this.effectAllowed_ = effect;
  }

  setDragImage(image, offsetX, offsetY) {
    this.dragImageData.image = image;
    this.dragImageData.offsetX = offsetX;
    this.dragImageData.offsetY = offsetY;
  }
}

suite('DragManager', () => {
  let delegate;
  let dragManager;
  let testTabsApiProxy;

  const tabs = [
    {
      active: true,
      alertStates: [],
      id: 0,
      index: 0,
      pinned: false,
      title: 'Tab 1',
    },
    {
      active: false,
      alertStates: [],
      id: 1,
      index: 1,
      pinned: false,
      title: 'Tab 2',
    },
  ];

  const strings = {
    tabIdDataType: 'application/tab-id',
  };

  /**
   * @param {!TabElement} tabElement
   * @param {string} groupId
   * @return {!TabGroupElement}
   */
  function groupTab(tabElement, groupId) {
    const groupElement = document.createElement('tabstrip-tab-group');
    groupElement.setAttribute('data-group-id', groupId);
    delegate.replaceChild(groupElement, tabElement);

    tabElement.tab = Object.assign({}, tabElement.tab, {groupId});
    groupElement.appendChild(tabElement);
    return groupElement;
  }

  setup(() => {
    loadTimeData.overrideValues(strings);
    testTabsApiProxy = new TestTabsApiProxy();
    TabsApiProxy.instance_ = testTabsApiProxy;

    delegate = new MockDelegate();
    tabs.forEach(tab => {
      const tabElement = document.createElement('tabstrip-tab');
      tabElement.tab = tab;
      delegate.appendChild(tabElement);
    });
    dragManager = new DragManager(delegate);
    dragManager.startObserving();
  });

  test('DragStartSetsDragImage', () => {
    const draggedTab = delegate.children[0];
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);
    assertEquals(dragStartEvent.dataTransfer.effectAllowed, 'move');
    assertEquals(
        mockDataTransfer.dragImageData.image, draggedTab.getDragImage());
    assertEquals(
        mockDataTransfer.dragImageData.offsetX, 100 - draggedTab.offsetLeft);
    assertEquals(
        mockDataTransfer.dragImageData.offsetY, 150 - draggedTab.offsetTop);
  });

  test('DragOverMovesTabs', async () => {
    const draggedIndex = 0;
    const dragOverIndex = 1;
    const draggedTab = delegate.children[draggedIndex];
    const dragOverTab = delegate.children[dragOverIndex];
    const mockDataTransfer = new MockDataTransfer();

    // Dispatch a dragstart event to start the drag process.
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Move the draggedTab over the 2nd tab.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    assertEquals(dragOverEvent.dataTransfer.dropEffect, 'move');

    // Dragover tab and dragged tab have now switched places in the DOM.
    assertEquals(draggedTab, delegate.children[dragOverIndex]);
    assertEquals(dragOverTab, delegate.children[draggedIndex]);

    draggedTab.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [tabId, newIndex] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(tabId, tabs[draggedIndex].id);
    assertEquals(newIndex, dragOverIndex);
  });

  test('DragTabOverTabGroup', async () => {
    const tabElements = delegate.children;

    // Group the first tab.
    const dragOverTabGroup = groupTab(tabElements[0], 'group0');

    // Start dragging the second tab.
    const draggedTab = tabElements[1];
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Drag the second tab over the newly created tab group.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTabGroup.dispatchEvent(dragOverEvent);

    // Tab is now in the group within the DOM.
    assertEquals(dragOverTabGroup, draggedTab.parentElement);

    draggedTab.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [tabId, groupId] = await testTabsApiProxy.whenCalled('groupTab');
    assertEquals(draggedTab.tab.id, tabId);
    assertEquals('group0', groupId);
  });

  test('DragTabOutOfTabGroup', async () => {
    // Group the first tab.
    const draggedTab = delegate.children[0];
    groupTab(draggedTab, 'group0');

    // Start dragging the first tab.
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Drag the first tab out.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragOverEvent);

    // The tab is now outside of the group in the DOM.
    assertEquals(delegate, draggedTab.parentElement);

    draggedTab.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [tabId] = await testTabsApiProxy.whenCalled('ungroupTab');
    assertEquals(draggedTab.tab.id, tabId);
  });

  test('DragGroupOverTab', async () => {
    const tabElements = delegate.children;

    // Start dragging the group.
    const draggedGroupIndex = 0;
    const draggedGroup = groupTab(tabElements[draggedGroupIndex], 'group0');
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.dispatchEvent(dragStartEvent);

    // Drag the group over the second tab.
    const dragOverIndex = 1;
    const dragOverTab = tabElements[dragOverIndex];
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);

    // Group and tab have now switched places.
    assertEquals(draggedGroup, delegate.children[dragOverIndex]);
    assertEquals(dragOverTab, delegate.children[draggedGroupIndex]);

    draggedGroup.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals('group0', groupId);
    assertEquals(1, index);
  });

  test('DragGroupOverGroup', async () => {
    const tabElements = delegate.children;

    // Group the first tab and second tab separately.
    const draggedIndex = 0;
    const draggedGroup = groupTab(tabElements[draggedIndex], 'group0');
    const dragOverIndex = 1;
    const dragOverGroup = groupTab(tabElements[dragOverIndex], 'group1');

    // Start dragging the first group.
    const mockDataTransfer = new MockDataTransfer();
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.dispatchEvent(dragStartEvent);

    // Drag the group over the second tab.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverGroup.dispatchEvent(dragOverEvent);

    // Groups have now switched places.
    assertEquals(draggedGroup, delegate.children[dragOverIndex]);
    assertEquals(dragOverGroup, delegate.children[draggedIndex]);

    draggedGroup.dispatchEvent(new DragEvent('drop', {bubbles: true}));
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals('group0', groupId);
    assertEquals(1, index);
  });

  test('DragExternalTabOverTab', async () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);

    // Test that a placeholder tab was created.
    const placeholderTabElement = delegate.lastElementChild;
    assertEquals(PLACEHOLDER_TAB_ID, placeholderTabElement.tab.id);

    const dragOverIndex = 0;
    const dragOverTab = delegate.children[dragOverIndex];
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    assertEquals(placeholderTabElement, delegate.children[dragOverIndex]);

    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dropEvent);
    assertEquals(externalTabId, placeholderTabElement.tab.id);
    const [tabId, index] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(externalTabId, tabId);
    assertEquals(dragOverIndex, index);
  });

  test('DragExternalTabOverTabGroup', async () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);
    const placeholderTabElement = delegate.lastElementChild;

    const draggedGroup = groupTab(delegate.children[0], 'group0');
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.dispatchEvent(dragOverEvent);
    assertEquals(draggedGroup, placeholderTabElement.parentElement);

    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    draggedGroup.dispatchEvent(dropEvent);
    const [tabId, groupId] = await testTabsApiProxy.whenCalled('groupTab');
    assertEquals(externalTabId, tabId);
    assertEquals('group0', groupId);
  });

  test('CancelDragResetsPosition', () => {
    const draggedIndex = 0;
    const draggedTab = delegate.children[draggedIndex];
    const mockDataTransfer = new MockDataTransfer();

    // Dispatch a dragstart event to start the drag process.
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Move the draggedTab over the 2nd tab.
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.children[1].dispatchEvent(dragOverEvent);

    draggedTab.dispatchEvent(new DragEvent('dragend', {bubbles: true}));
    assertEquals(draggedTab, delegate.children[draggedIndex]);
  });

  test('DragLeaveRemovesExternalTab', () => {
    const externalTabId = 1000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, `${externalTabId}`);
    const dragEnterEvent = new DragEvent('dragenter', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragEnterEvent);
    assertTrue(
        !!delegate.querySelector(`[data-tab-id="${PLACEHOLDER_TAB_ID}"]`));

    const dragLeaveEvent = new DragEvent('dragleave', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragLeaveEvent);
    assertFalse(
        !!delegate.querySelector(`[data-tab-id="${PLACEHOLDER_TAB_ID}"]`));
  });
});
