// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Root element of the OOBE UI Debugger.
 */


cr.define('cr.ui.login.debug', function() {
  const DEBUG_BUTTON_STYLE = `
      height:20px;
      width:120px;
      position: absolute;
      top: 0;
      left: calc(50% - 60px);
      background-color: red;
      color: white;
      z-index: 10001;
      text-align: center;`;

  const DEBUG_OVERLAY_STYLE = `
      position: absolute;
      top: 0;
      left: 0;
      height: 100%;
      width: 100%;
      background-color: rgba(0, 0, 255, 0.75);
      color: white;
      z-index: 10000;
      padding: 20px;
      display: flex;
      flex-direction: column;`;

  const TOOL_PANEL_STYLE = `
      direction: ltr;
      display: flex;
      flex-direction: row;`;

  const TOOL_BUTTON_STYLE = `
      border: 2px solid white;
      padding: 3px 10px;
      margin: 0 4px;`;

  class DebugButton {
    constructor(parent, title, callback) {
      let div =
          /** @type {!HTMLElement} */ (document.createElement('div'));
      div.textContent = title;

      div.setAttribute('style', TOOL_BUTTON_STYLE);
      div.addEventListener('click', this.onClick.bind(this));

      parent.appendChild(div);

      this.callback_ = callback;
      // In most cases we want to hide debugger UI right after making some
      // change to OOBE UI. However, more complex scenarios might want to
      // override this behavior.
      this.postCallback_ = function() {
        cr.ui.login.debug.DebuggerUI.getInstance().hideDebugUI();
      };
    }

    onClick() {
      if (this.callback_) {
        this.callback_();
      }
      if (this.postCallback_) {
        this.postCallback_();
      }
    }
  }

  class ToolPanel {
    constructor(parent, title) {
      let titleDiv =
          /** @type {!HTMLElement} */ (document.createElement('h1'));
      titleDiv.textContent = title;

      let panel = /** @type {!HTMLElement} */ (document.createElement('div'));
      panel.setAttribute('style', TOOL_PANEL_STYLE);

      parent.appendChild(titleDiv);
      parent.appendChild(panel);
      this.content = panel;
    }
  }

  class DebuggerUI {
    constructor() {
      this.debuggerVisible_ = false;
    }

    showDebugUI() {
      this.debuggerVisible_ = true;
      this.debuggerOverlay_.removeAttribute('hidden');
    }

    hideDebugUI() {
      this.debuggerVisible_ = false;
      this.debuggerOverlay_.setAttribute('hidden', true);
    }

    toggleDebugUI() {
      if (this.debuggerVisible_) {
        this.hideDebugUI();
      } else {
        this.showDebugUI();
      }
    }

    createLanguagePanel(parent) {
      let langPanel = new ToolPanel(this.debuggerOverlay_, 'Language');
      const LANGUAGES = [
        ['English', 'en-US'],
        ['German', 'de'],
        ['Russian', 'ru'],
        ['Herbew (RTL)', 'he'],
        ['Arabic (RTL)', 'ar'],
        ['Chinese', 'zh-TW'],
        ['Japanese', 'ja'],
      ];
      LANGUAGES.forEach(function(pair) {
        new DebugButton(langPanel.content, pair[0], function(locale) {
          chrome.send('WelcomeScreen.setLocaleId', [locale]);
        }.bind(null, pair[1]));
      });
    }

    register(element) {
      {
        // Create UI Debugger button
        let button =
            /** @type {!HTMLElement} */ (document.createElement('div'));
        button.setAttribute('style', DEBUG_BUTTON_STYLE);
        button.textContent = 'Debug';
        button.addEventListener('click', this.toggleDebugUI.bind(this));

        this.debuggerButton_ = button;
      }
      {
        // Create base debugger panel.
        let overlay =
            /** @type {!HTMLElement} */ (document.createElement('div'));
        overlay.setAttribute('style', DEBUG_OVERLAY_STYLE);
        overlay.setAttribute('hidden', true);
        this.debuggerOverlay_ = overlay;
      }
      this.createLanguagePanel(this.debuggerOverlay_);

      element.appendChild(this.debuggerButton_);
      element.appendChild(this.debuggerOverlay_);
    }
  }

  cr.addSingletonGetter(DebuggerUI);

  // Export
  return {
    DebuggerUI: DebuggerUI,
  };
});

/**
 * Initializes the Debugger. Called after the DOM, and all external scripts,
 * have been loaded.
 */
function initializeDebugger() {
  if (document.readyState === 'loading')
    return;
  document.removeEventListener('DOMContentLoaded', initializeDebugger);
  cr.ui.login.debug.DebuggerUI.getInstance().register(document.body);
}

/**
 * Final initialization performed after DOM and all scripts have loaded.
 */
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initializeDebugger);
} else {
  initializeDebugger();
}
