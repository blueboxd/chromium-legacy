// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Handles the Switch Access menu panel.
 * @implements {PanelInterface}
 */
class Panel {
  constructor() {
    /**
     * The menu manager.
     * @private {MenuManager}
     */
    this.menuManager_;

    /**
     * Reference to switch access.
     * @private {SwitchAccessInterface}
     */
    this.switchAccess_;

    /**
     * Reference to the menu panel element.
     * @private {Element}
     */
    this.panel_;

    /**
     * ID of the current menu being shown in the menu panel.
     * @private {?SAConstants.MenuId}
     */
    this.currentMenuId_;
  }

  /**
   * Initialize the panel and buttons.
   */
  init() {
    this.panel_ = document.getElementById(SAConstants.MENU_PANEL_ID);

    const buttons = document.getElementsByTagName('button');
    for (const button of buttons) {
      this.setupButton_(button);
    }

    const background = chrome.extension.getBackgroundPage();
    if (background.document.readyState === 'complete')
      this.connectToBackground();
    else
      background.addEventListener('load', this.connectToBackground.bind(this));

    this.addTranslatedMessagesToDom_();
  }

  /**
   * Once both the menu panel and the background page have loaded, pass a
   * reference to this object for communication.
   */
  connectToBackground() {
    this.switchAccess_ = chrome.extension.getBackgroundPage().switchAccess;
    this.menuManager_ = this.switchAccess_.connectMenuPanel(this);
  }

  /**
   * Adds an event listener to the given button to send a message when clicked.
   * @param {!Element} button
   * @private
   */
  setupButton_(button) {
    let action = button.id;
    button.addEventListener('click', function(action) {
      this.menuManager_.performAction(action);
    }.bind(this, action));
  }

  /**
   * Temporary function, until multiple focus rings is implemented.
   * Puts a focus ring around the given menu item.
   * TODO(crbug/925103): Implement multiple focus rings.
   *
   * @param {string} id
   * @param {boolean} enable
   */
  setFocusRing(id, enable) {
    this.updateClass_(id, SAConstants.Focus.CLASS, enable);
    return;
  }

  /**
   * Sets which buttons are enabled/disabled, based on |actions|.
   * @param {!Array<string>} actions
   */
  setActions(actions) {
    const div = document.getElementById(SAConstants.MENU_PANEL_ID);
    for (const button of div.children)
      button.hidden = !actions.includes(button.id);

    this.setHeight_(actions.length);
  }

  /**
   * Sets the actions in the menu panel to the actions in |actions| from
   * the menu with the given |menuId|.
   * TODO(sophyang): Replace setActions() with this function once
   * submenus are implemented.
   * @param {!Array<string>} actions
   * @param {!SAConstants.MenuId} menuId
   * @public
   */
  setActionsFromMenu(actions, menuId) {
    const menu = document.getElementById(menuId);
    const menuButtons = Array.from(menu.children);

    // Add the menu to the panel if it is not already being shown.
    if (menuId !== this.currentMenuId_) {
      this.panel_.clear();
      this.panel_.appendChild(menu);
    }

    // Hide menu actions not applicable to the current node.
    for (const button of menuButtons) {
      button.hidden = !actions.includes(button.id);
    }

    this.currentMenuId_ = menuId;

    this.setHeight_(actions.length);
  }

  /**
   * Clears the current menu from the panel.
   * @public
   */
  clear() {
    if (this.currentMenuId_) {
      const menu = document.getElementById(this.currentMenuId_);
      document.body.appendChild(menu);

      this.currentMenuId_ = null;
    }
  }

  /**
   * Get the id of the current menu being shown in the panel. A null
   * id indicates that no menu is currently being shown in the panel.
   * @return {?SAConstants.MenuId}
   * @public
   */
  currentMenuId() {
    return this.currentMenuId_;
  }

  /**
   * Either adds or removes the class |className| for the element with the given
   * |id|.
   * @param {string} id
   * @param {string} className
   * @param {boolean} shouldAdd
   */
  updateClass_(id, className, shouldAdd) {
    const htmlNode = document.getElementById(id);
    if (shouldAdd)
      htmlNode.classList.add(className);
    else
      htmlNode.classList.remove(className);
  }

  /**
   * Sets the height of the menu (minus the body padding) based on the number of
   * actions in the menu. This is necessary because floated elements do not
   * contribute to their parent's height, and the elements are floated to avoid
   * arbitrary space being added between buttons.
   *
   * @param {number} numActions
   */
  setHeight_(numActions) {
    // TODO(anastasi): This should be a preference that the user can change.
    const maxCols = 3;
    const numRows = Math.ceil(numActions / maxCols);

    let rowHeight;

    if (this.switchAccess_.improvedTextInputEnabled()) {
      rowHeight = 85;
      const actions = document.getElementsByClassName('action');
      for (let action of actions) {
        action.classList.add('improvedTextInputEnabled');
      }
    } else {
      rowHeight = 60;
    }

    const height = rowHeight * numRows;
    this.panel_.style.height = height + 'px';
  }

  /**
   * Processes an HTML DOM, replacing text content with translated text messages
   * on elements marked up for translation. Elements whose class attributes
   * contain the 'i18n' class name are expected to also have an msgid attribute.
   * The value of the msgid attributes are looked up as message IDs and the
   * resulting text is used as the text content of the elements.
   *
   * TODO(crbug/706981): Combine with similar function in SelectToSpeakOptions.
   * @private
   */
  addTranslatedMessagesToDom_() {
    const elements = document.querySelectorAll('.i18n');
    for (const element of elements) {
      const messageId = element.getAttribute('msgid');
      if (!messageId)
        throw new Error('Element has no msgid attribute: ' + element);
      const translatedMessage =
          chrome.i18n.getMessage('switch_access_' + messageId);
      if (element.tagName == 'INPUT')
        element.setAttribute('placeholder', translatedMessage);
      else
        element.textContent = translatedMessage;
      element.classList.add('i18n-processed');
    }
  }
}

let switchAccessMenuPanel = new Panel();

if (document.readyState === 'complete')
  switchAccessMenuPanel.init();
else
  window.addEventListener(
      'load', switchAccessMenuPanel.init.bind(switchAccessMenuPanel));
