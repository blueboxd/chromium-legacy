// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {DragManager} from 'chrome://tab-strip/drag_manager.js';
import {TabElement} from 'chrome://tab-strip/tab.js';
import {TabGroupElement} from 'chrome://tab-strip/tab_group.js';
import {TabsApiProxy} from 'chrome://tab-strip/tabs_api_proxy.js';

import {TestTabsApiProxy} from './test_tabs_api_proxy.js';

class MockDelegate extends HTMLElement {
  constructor() {
    super();
  }

  getIndexOfTab(tabElement) {
    return Array.from(this.querySelectorAll('tabstrip-tab'))
        .indexOf(tabElement);
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
    delegate.addEventListener('dragstart', e => dragManager.startDrag(e));
    delegate.addEventListener('dragend', e => dragManager.stopDrag(e));
    delegate.addEventListener('dragleave', () => dragManager.cancelDrag());
    delegate.addEventListener('dragover', (e) => dragManager.continueDrag(e));
    delegate.addEventListener('drop', (e) => dragManager.drop(e));
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

    // Dispatch a dragstart event to start the drag process
    const dragStartEvent = new DragEvent('dragstart', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    draggedTab.dispatchEvent(dragStartEvent);

    // Move the draggedTab over the 2nd tab
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    assertEquals(dragOverEvent.dataTransfer.dropEffect, 'move');
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
    const [tabId] = await testTabsApiProxy.whenCalled('ungroupTab');
    assertEquals(draggedTab.tab.id, tabId);
  });

  test('DragGroupOverTab', async () => {
    const tabElements = delegate.children;

    // Start dragging the group.
    const draggedGroup = groupTab(tabElements[0], 'group0');
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
    const dragOverTab = tabElements[1];
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    dragOverTab.dispatchEvent(dragOverEvent);
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals('group0', groupId);
    assertEquals(1, index);
  });

  test('DragGroupOverGroup', async () => {
    const tabElements = delegate.children;

    // Group the first tab and second tab separately.
    const draggedGroup = groupTab(tabElements[0], 'group0');
    const dragOverGroup = groupTab(tabElements[1], 'group1');

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
    const [groupId, index] = await testTabsApiProxy.whenCalled('moveGroup');
    assertEquals('group0', groupId);
    assertEquals(1, index);
  });

  test('DragTabIntoListFromOutside', () => {
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, '1000');
    const dragOverEvent = new DragEvent('dragover', {
      bubbles: true,
      composed: true,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dragOverEvent);
    assertTrue(delegate.lastElementChild.id === 'dropPlaceholder');

    delegate.dispatchEvent(new DragEvent('drop', dragOverEvent));
    assertEquals(null, delegate.querySelector('#dropPlaceholder'));
  });

  test('DropTabIntoList', async () => {
    const droppedTabId = 9000;
    const mockDataTransfer = new MockDataTransfer();
    mockDataTransfer.setData(strings.tabIdDataType, droppedTabId);
    const dropEvent = new DragEvent('drop', {
      bubbles: true,
      composed: true,
      clientX: 100,
      clientY: 150,
      dataTransfer: mockDataTransfer,
    });
    delegate.dispatchEvent(dropEvent);

    const [tabId, index] = await testTabsApiProxy.whenCalled('moveTab');
    assertEquals(droppedTabId, tabId);
    assertEquals(-1, index);
  });
});
