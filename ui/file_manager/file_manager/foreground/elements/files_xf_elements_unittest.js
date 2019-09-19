// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Adds a xf-display-panel element to the test page.
 */
function setUpPage() {
  document.body.innerHTML +=
      '<xf-display-panel id="test-xf-display-panel"></xf-display-panel>';
}

async function testFilesDisplayPanel(done) {
  // Get the host display panel container element.
  /** @type {!DisplayPanel|!Element} */
  const displayPanel = assert(document.querySelector('#test-xf-display-panel'));

  // Declare the expected panel item height managed from CSS.
  const singlePanelHeight = 68;

  // Add a progress panel item to the display panel container.
  let progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Confirm the expected height of a single panelItem.
  let bounds = progressPanel.getBoundingClientRect();
  assert(bounds.height === singlePanelHeight);

  // Add second panel item.
  progressPanel = displayPanel.addPanelItem('testpanel2');
  progressPanel.panelType = progressPanel.panelTypeProgress;

  // Confirm multiple progress panel items are hidden by default.
  const panelContainer = displayPanel.shadowRoot.querySelector('#panels');
  assertTrue(panelContainer.hasAttribute('hidden'));

  // Confirm multiple progress panels cause creation of a summary panel.
  const summaryContainer = displayPanel.shadowRoot.querySelector('#summary');
  let summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertTrue(summaryPanelItem.panelType === summaryPanelItem.panelTypeSummary);

  // Confirm the expected height of the summary panel.
  bounds = summaryPanelItem.getBoundingClientRect();
  assert(bounds.height === singlePanelHeight);

  // Trigger expand of the summary panel.
  let expandButton =
      summaryPanelItem.shadowRoot.querySelector('#primary-action');
  expandButton.click();

  // Confirm the panel container is no longer hidden.
  assertFalse(panelContainer.hasAttribute('hidden'));

  // Remove a progress panel and ensure the summary panel is removed.
  let panelToRemove = displayPanel.findPanelItemById('testpanel1');
  displayPanel.removePanelItem(panelToRemove);
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertTrue(summaryPanelItem === null);

  // Confirm the reference to the removed panel item is gone.
  assertTrue(displayPanel.findPanelItemById('testpanel1') === null);

  // Add another panel item and confirm the expanded state persists by
  // checking the pnel container is not hidden and there is a summary panel.
  progressPanel = displayPanel.addPanelItem('testpanel1');
  progressPanel.panelType = progressPanel.panelTypeProgress;
  assertFalse(panelContainer.hasAttribute('hidden'));
  summaryPanelItem = summaryContainer.querySelector('xf-panel-item');
  assertTrue(summaryPanelItem.panelType === summaryPanelItem.panelTypeSummary);

  done();
}
