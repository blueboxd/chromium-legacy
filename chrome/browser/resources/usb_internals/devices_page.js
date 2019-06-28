// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for DevicesPage, served from
 *     chrome://usb-internals/.
 */

cr.define('devices_page', function() {
  const UsbDeviceProxy = device.mojom.UsbDeviceProxy;

  /**
   * Page that contains a tab header and a tab panel displaying devices table.
   */
  class DevicesPage {
    /**
     * @param {!device.mojom.UsbDeviceManagerProxy} usbManager
     */
    constructor(usbManager) {
      /** @private {device.mojom.UsbDeviceManagerProxy} */
      this.usbManager_ = usbManager;
      this.renderDeviceList_();
    }

    /**
     * Sets the device collection for the page's device table.
     * @private
     */
    async renderDeviceList_() {
      const response = await this.usbManager_.getDevices();

      /** @type {!Array<!device.mojom.UsbDeviceInfo>} */
      const devices = response.results;

      const tableBody = $('device-list');
      tableBody.innerHTML = '';

      const rowTemplate = document.querySelector('#device-row');

      for (const device of devices) {
        const clone = document.importNode(rowTemplate.content, true);

        const td = clone.querySelectorAll('td');

        td[0].textContent = device.busNumber;
        td[1].textContent = device.portNumber;
        td[2].textContent = toHex(device.vendorId);
        td[3].textContent = toHex(device.productId);
        if (device.manufacturerName) {
          td[4].textContent = decodeString16(device.manufacturerName.data);
        }
        if (device.productName) {
          td[5].textContent = decodeString16(device.productName.data);
        }
        if (device.serialNumber) {
          td[6].textContent = decodeString16(device.serialNumber.data);
        }

        const inspectButton = clone.querySelector('button');
        inspectButton.addEventListener('click', () => {
          this.switchToTab_(device);
        });

        tableBody.appendChild(clone);
      }
      // window.deviceListCompleteFn() provides a hook for the test suite to
      // perform test actions after the devices list is loaded.
      window.deviceListCompleteFn();
    }

    /**
     * Switches to the device's tab, creating one if necessary.
     * @param {!device.mojom.UsbDeviceInfo} device
     * @private
     */
    switchToTab_(device) {
      const tabId = device.guid;

      if (null == $(tabId)) {
        const devicePage = new DevicePage(this.usbManager_, device);
      }
      $(tabId).selected = true;
    }
  }

  /**
   * Page that contains a tree view displaying devices detail and can get
   * descriptors.
   */
  class DevicePage {
    /**
     * @param {!device.mojom.UsbDeviceManagerProxy} usbManager
     * @param {!device.mojom.UsbDeviceInfo} device
     */
    constructor(usbManager, device) {
      /** @private {device.mojom.UsbDeviceManagerProxy} */
      this.usbManager_ = usbManager;
      this.renderTab_(device);
    }

    /**
     * Renders a tab to display a tree view showing device's detail information.
     * @param {!device.mojom.UsbDeviceInfo} device
     * @private
     */
    renderTab_(device) {
      const tabs = document.querySelector('tabs');

      const tabTemplate = document.querySelector('#tab-template');
      const tabClone = document.importNode(tabTemplate.content, true);

      const tab = tabClone.querySelector('tab');
      if (device.productName) {
        tab.textContent = decodeString16(device.productName.data);
      } else {
        const vendorId = toHex(device.vendorId).slice(2);
        const productId = toHex(device.productId).slice(2);
        tab.textContent = `${vendorId}:${productId}`;
      }
      tab.id = device.guid;

      tabs.appendChild(tabClone);
      cr.ui.decorate('tab', cr.ui.Tab);

      const tabPanels = document.querySelector('tabpanels');
      const tabPanelTemplate =
          document.querySelector('#device-tabpanel-template');
      const tabPanelClone = document.importNode(tabPanelTemplate.content, true);

      /**
       * Root of the WebContents tree of current device.
       * @type {?cr.ui.Tree}
       */
      const treeViewRoot = tabPanelClone.querySelector('.tree-view');
      cr.ui.decorate(treeViewRoot, cr.ui.Tree);
      treeViewRoot.detail = {payload: {}, children: {}};
      // Clear the tree first before populating it with the new content.
      treeViewRoot.innerText = '';
      renderDeviceTree(device, treeViewRoot);

      const tabPanel = tabPanelClone.querySelector('tabpanel');
      this.initializeDescriptorPanels_(tabPanel, device.guid);

      tabPanels.appendChild(tabPanelClone);
      cr.ui.decorate(tabPanel, cr.ui.TabPanel);
    }

    /**
     * Initializes all the descriptor panels.
     * @param {!HTMLElement} tabPanel
     * @param {string} guid
     * @private
     */
    async initializeDescriptorPanels_(tabPanel, guid) {
      const usbDeviceProxy = new UsbDeviceProxy;
      await this.usbManager_.getDevice(guid, usbDeviceProxy.$.createRequest());

      const deviceDescriptorPanel = initialInspectorPanel(
          tabPanel, 'device-descriptor', usbDeviceProxy, guid);

      const configurationDescriptorPanel = initialInspectorPanel(
          tabPanel, 'configuration-descriptor', usbDeviceProxy, guid);

      const stringDescriptorPanel = initialInspectorPanel(
          tabPanel, 'string-descriptor', usbDeviceProxy, guid);
      deviceDescriptorPanel.setStringDescriptorPanel(stringDescriptorPanel);
      configurationDescriptorPanel.setStringDescriptorPanel(
          stringDescriptorPanel);

      initialInspectorPanel(tabPanel, 'bos-descriptor', usbDeviceProxy, guid);

      initialInspectorPanel(tabPanel, 'testing-tool', usbDeviceProxy, guid);

      // window.deviceTabInitializedFn() provides a hook for the test suite to
      // perform test actions after the device tab query descriptors actions are
      // initialized.
      window.deviceTabInitializedFn();
    }
  }

  /**
   * Renders a tree to display the device's detail information.
   * @param {!device.mojom.UsbDeviceInfo} device
   * @param {!cr.ui.Tree} root
   */
  function renderDeviceTree(device, root) {
    root.add(customTreeItem(`USB Version: ${device.usbVersionMajor}.${
        device.usbVersionMinor}.${device.usbVersionSubminor}`));

    root.add(customTreeItem(`Class Code: ${device.classCode}`));

    root.add(customTreeItem(`Subclass Code: ${device.subclassCode}`));

    root.add(customTreeItem(`Protocol Code: ${device.protocolCode}`));

    root.add(customTreeItem(`Port Number: ${device.portNumber}`));

    root.add(customTreeItem(`Vendor Id: ${toHex(device.vendorId)}`));

    root.add(customTreeItem(`Product Id: ${toHex(device.productId)}`));

    root.add(customTreeItem(`Device Version: ${device.deviceVersionMajor}.${
        device.deviceVersionMinor}.${device.deviceVersionSubminor}`));

    if (device.manufacturerName) {
      root.add(customTreeItem(`Manufacturer Name: ${
          decodeString16(device.manufacturerName.data)}`));
    }

    if (device.productName) {
      root.add(customTreeItem(
          `Product Name: ${decodeString16(device.productName.data)}`));
    }

    if (device.serialNumber) {
      root.add(customTreeItem(
          `Serial Number: ${decodeString16(device.serialNumber.data)}`));
    }

    if (device.webusbLandingPage) {
      const urlItem = customTreeItem(
          `WebUSB Landing Page: ${device.webusbLandingPage.url}`);
      root.add(urlItem);

      urlItem.querySelector('.tree-label')
          .addEventListener(
              'click',
              () => window.open(device.webusbLandingPage.url, '_blank'));
    }

    root.add(
        customTreeItem(`Active Configuration: ${device.activeConfiguration}`));

    const configurationsArray = device.configurations;
    renderConfigurationTreeItem(configurationsArray, root);
  }

  /**
   * Renders a tree item to display the device's configuration information.
   * @param {!Array<!device.mojom.UsbConfigurationInfo>} configurationsArray
   * @param {!cr.ui.TreeItem} root
   */
  function renderConfigurationTreeItem(configurationsArray, root) {
    for (const configuration of configurationsArray) {
      const configurationItem =
          customTreeItem(`Configuration ${configuration.configurationValue}`);

      if (configuration.configurationName) {
        configurationItem.add(customTreeItem(`Configuration Name: ${
            decodeString16(configuration.configurationName.data)}`));
      }

      const interfacesArray = configuration.interfaces;
      renderInterfacesTreeItem(interfacesArray, configurationItem);

      root.add(configurationItem);
    }
  }

  /**
   * Renders a tree item to display the device's interface information.
   * @param {!Array<!device.mojom.UsbInterfaceInfo>} interfacesArray
   * @param {!cr.ui.TreeItem} root
   */
  function renderInterfacesTreeItem(interfacesArray, root) {
    for (const currentInterface of interfacesArray) {
      const interfaceItem =
          customTreeItem(`Interface ${currentInterface.interfaceNumber}`);

      const alternatesArray = currentInterface.alternates;
      renderAlternatesTreeItem(alternatesArray, interfaceItem);

      root.add(interfaceItem);
    }
  }

  /**
   * Renders a tree item to display the device's alternate interfaces
   * information.
   * @param {!Array<!device.mojom.UsbAlternateInterfaceInfo>} alternatesArray
   * @param {!cr.ui.TreeItem} root
   */
  function renderAlternatesTreeItem(alternatesArray, root) {
    for (const alternate of alternatesArray) {
      const alternateItem =
          customTreeItem(`Alternate ${alternate.alternateSetting}`);

      alternateItem.add(customTreeItem(`Class Code: ${alternate.classCode}`));

      alternateItem.add(
          customTreeItem(`Subclass Code: ${alternate.subclassCode}`));

      alternateItem.add(
          customTreeItem(`Protocol Code: ${alternate.protocolCode}`));

      if (alternate.interfaceName) {
        alternateItem.add(customTreeItem(
            `Interface Name: ${decodeString16(alternate.interfaceName.data)}`));
      }

      const endpointsArray = alternate.endpoints;
      renderEndpointsTreeItem(endpointsArray, alternateItem);

      root.add(alternateItem);
    }
  }

  /**
   * Renders a tree item to display the device's endpoints information.
   * @param {!Array<!device.mojom.UsbEndpointInfo>} endpointsArray
   * @param {!cr.ui.TreeItem} root
   */
  function renderEndpointsTreeItem(endpointsArray, root) {
    for (const endpoint of endpointsArray) {
      let itemLabel = 'Endpoint ';

      itemLabel += endpoint.endpointNumber;

      switch (endpoint.direction) {
        case device.mojom.UsbTransferDirection.INBOUND:
          itemLabel += ' (INBOUND)';
          break;
        case device.mojom.UsbTransferDirection.OUTBOUND:
          itemLabel += ' (OUTBOUND)';
          break;
      }

      const endpointItem = customTreeItem(itemLabel);

      let usbTransferType = '';
      switch (endpoint.type) {
        case device.mojom.UsbTransferType.CONTROL:
          usbTransferType = 'CONTROL';
          break;
        case device.mojom.UsbTransferType.ISOCHRONOUS:
          usbTransferType = 'ISOCHRONOUS';
          break;
        case device.mojom.UsbTransferType.BULK:
          usbTransferType = 'BULK';
          break;
        case device.mojom.UsbTransferType.INTERRUPT:
          usbTransferType = 'INTERRUPT';
          break;
      }

      endpointItem.add(customTreeItem(`USB Transfer Type: ${usbTransferType}`));

      endpointItem.add(customTreeItem(`Packet Size: ${endpoint.packetSize}`));

      root.add(endpointItem);
    }
  }

  /**
   * Initialize a descriptor panel.
   * @param {!HTMLElement} tabPanel
   * @param {string} panelType
   * @param {!device.mojom.UsbDeviceInterface} usbDeviceProxy
   * @param {string} guid
   * @return {!descriptor_panel.DescriptorPanel}
   */
  function initialInspectorPanel(tabPanel, panelType, usbDeviceProxy, guid) {
    const button = tabPanel.querySelector(`.${panelType}-button`);
    const displayElement = tabPanel.querySelector(`.${panelType}-panel`);
    const descriptorPanel =
        new descriptor_panel.DescriptorPanel(usbDeviceProxy, displayElement);
    switch (panelType) {
      case 'string-descriptor':
        descriptorPanel.initialStringDescriptorPanel(guid);
        break;
      case 'testing-tool':
        descriptorPanel.initialTestingToolPanel();
        break;
    }

    button.addEventListener('click', async () => {
      displayElement.hidden = !displayElement.hidden;
      // Clear the panel before rendering new data.
      descriptorPanel.clearView();

      if (!displayElement.hidden) {
        switch (panelType) {
          case 'device-descriptor':
            await descriptorPanel.getDeviceDescriptor();
            break;
          case 'configuration-descriptor':
            await descriptorPanel.getConfigurationDescriptor();
            break;
          case 'string-descriptor':
            await descriptorPanel.getAllLanguageCodes();
            break;
          case 'bos-descriptor':
            await descriptorPanel.getBosDescriptor();
            break;
        }
      }
    });
    return descriptorPanel;
  }

  /**
   * Parses utf16 coded string.
   * @param {!mojoBase.mojom.String16} arr
   * @return {string}
   */
  function decodeString16(arr) {
    return arr.map(ch => String.fromCodePoint(ch)).join('');
  }

  /**
   * Parses the decimal number to hex format.
   * @param {number} num
   * @return {string}
   */
  function toHex(num) {
    return `0x${num.toString(16).padStart(4, '0').toUpperCase()}`;
  }

  /**
   * Renders a customized TreeItem with the given content and class name.
   * @param {string} itemLabel
   * @return {!cr.ui.TreeItem}
   * @private
   */
  function customTreeItem(itemLabel) {
    return item = new cr.ui.TreeItem({
      label: itemLabel,
      icon: '',
    });
  }

  return {
    DevicesPage,
  };
});

window.deviceListCompleteFn = window.deviceListCompleteFn || function() {};

window.deviceTabInitializedFn = window.deviceTabInitializedFn || function() {};