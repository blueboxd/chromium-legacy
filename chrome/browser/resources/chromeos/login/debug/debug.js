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
      right: 0;
      bottom : 0;
      background-color: rgba(0, 0, 255, 0.90);
      color: white;
      z-index: 10000;
      padding: 20px;
      display: flex;
      flex-direction: column;`;

  const TOOL_PANEL_STYLE = `
      direction: ltr;
      display: flex;
      flex-wrap: wrap;
      flex-direction: row;`;

  const TOOL_BUTTON_STYLE = `
      border: 2px solid white;
      padding: 3px 10px;
      margin: 3px 4px;
      text-align: center;
      vertical-align: middle;
      font-weight: bold;`;

  const TOOL_BUTTON_SELECTED_STYLE = `
      border-width: 4px !important;
      padding: 2px 9px !important;
      margin: 2px 3px !important;`;

  // White.
  const SCREEN_BUTTON_STYLE_NORMAL = `
      border-color: #fff !important;
      color: #fff;`;
  // Orange-tinted.
  const SCREEN_BUTTON_STYLE_ERROR = `
      border-color: #f80 !important;
      color: #f80`;
  // Green-tinted.
  const SCREEN_BUTTON_STYLE_OTHER = `
      border-color: #afa !important;
      color: #afa`;
  // Pink-tinted.
  const SCREEN_BUTTON_STYLE_UNKNOWN = `
      border-color: #faa !important;
      color: #faa`;

  /**
   * Indicates if screen is present in usual user flow, represents some error
   * state or is shown in some other cases. See KNOWN_SCREENS for more details.
   * @enum {string}
   */
  const ScreenKind = {
    NORMAL: 'normal',
    ERROR: 'error',
    OTHER: 'other',
    UNKNOWN: 'unknown',
  };

  /**
   * List of possible screens.
   * Attributes:
   *    id - ID of the screen passed to login.createScreen
   *    kind - Used for button coloring, indicates if screen is present in
   *           normal flow (via debug-button-<kind> classes).
   *    suffix - extra text to display on button, gives some insight
   *             on context in which screen is shown (e.g. Enterprise-specific
   *             screens).
   *    data - extra data passed to DisplayManager.showScreen.
   */
  const KNOWN_SCREENS = [
    {
      // Device without mouse/keyboard.
      id: 'hid-detection',
      kind: ScreenKind.NORMAL,
    },
    {
      // Welcome screen.
      id: 'connect',
      kind: ScreenKind.NORMAL,
      data: {
        isDeveloperMode: true,
      }
    },
    {
      id: 'debugging',
      kind: ScreenKind.OTHER,
    },
    {
      id: 'demo-preferences',
      kind: ScreenKind.OTHER,
      suffix: 'demo',
    },
    {
      id: 'network-selection',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'eula',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'demo-setup',
      kind: ScreenKind.OTHER,
      suffix: 'demo',
    },
    {
      id: 'update',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'auto-enrollment-check',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'device-disabled',
      kind: ScreenKind.ERROR,
      suffix: 'E',
    },
    {
      id: 'oauth-enrollment',
      kind: ScreenKind.NORMAL,
      suffix: 'E',
    },
    {
      id: 'packaged-license',
      kind: ScreenKind.NORMAL,
      suffix: 'E',
    },
    // Login screen starts here.
    {
      id: 'error-message',
      kind: ScreenKind.ERROR,
    },
    {
      id: 'update-required',
      kind: ScreenKind.OTHER,
      suffix: 'E',
    },
    {
      id: 'autolaunch',
      kind: ScreenKind.OTHER,
      suffix: 'kiosk',
    },
    {
      id: 'app-launch-splash',
      kind: ScreenKind.OTHER,
      suffix: 'kiosk',
      data: {
        appInfo: {
          name: 'Application Name',
          url: 'http://example.com/someApplication/VeryLongURL',
          iconURL: 'chrome://theme/IDR_LOGO_GOOGLE_COLOR_90',
        },
        shortcutEnabled: true,
      }
    },
    {
      id: 'wrong-hwid',
      kind: ScreenKind.ERROR,
    },
    {
      id: 'reset',
      kind: ScreenKind.OTHER,
    },

    {
      // Shown instead of sign-in screen, triggered from Crostini.
      id: 'adb-sideloading',
      kind: ScreenKind.OTHER,
    },
    {
      // Customer kiosk feature.
      id: 'kiosk-enable',
      kind: ScreenKind.OTHER,
    },
    {
      id: 'account-picker',
      kind: ScreenKind.OTHER,
      suffix: 'multiuser',
    },
    {
      id: 'gaia-signin',
      kind: ScreenKind.NORMAL,
      states: [
        {
          // Generic offline GAIA
          id: 'offline-gaia',
          trigger: (screen) => {
            screen.loadAuthExtension({
              screenMode: 1,  // Offline
            });
          }
        },
        {
          // kAccountsPrefLoginScreenDomainAutoComplete value is set
          id: 'offline-gaia-domain',
          trigger: (screen) => {
            screen.loadAuthExtension({
              screenMode: 1,  // Offline
              emailDomain: 'somedomain.com',
            });
          }
        },
        {
          // Device is enterprise-managed.
          id: 'offline-gaia-enterprise',
          trigger: (screen) => {
            screen.loadAuthExtension({
              screenMode: 1,  // Offline
              enterpriseDisplayDomain: 'example.com',
            });
          }
        },
        {
          // Retry after incorrect password attempt, user name is already known.
          id: 'offline-gaia-user',
          trigger: (screen) => {
            screen.loadAuthExtension({
              screenMode: 1,  // Offline
              email: 'someone@example.com',
            });
          }
        },
      ]
    },
    {
      id: 'tpm-error-message',
      kind: ScreenKind.ERROR,
    },
    {
      // Failure during SAML flow.
      id: 'fatal-error',
      kind: ScreenKind.ERROR,
      suffix: 'SAML',
    },
    {
      // GAIA password changed.
      id: 'password-changed',
      kind: ScreenKind.OTHER,
    },
    {
      id: 'ad-password-change',
      kind: ScreenKind.OTHER,
      data: {
        username: 'username',
        error: 1,
      }
    },
    {
      id: 'encryption-migration',
      kind: ScreenKind.OTHER,
    },
    {
      id: 'confirm-password',
      kind: ScreenKind.OTHER,
      suffix: 'SAML',
    },
    {
      id: 'supervision-transition',
      kind: ScreenKind.OTHER,
    },
    {
      id: 'terms-of-service',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'sync-consent',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'fingerprint-setup',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'discover',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'arc-tos',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'recommend-apps',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'app-downloading',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'assistant-optin-flow',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'multidevice-setup',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'gesture-navigation',
      kind: ScreenKind.NORMAL,
    },
    {
      id: 'marketing-opt-in',
      kind: ScreenKind.NORMAL,
    },
  ];

  class DebugButton {
    constructor(parent, title, callback) {
      this.element =
          /** @type {!HTMLElement} */ (document.createElement('div'));
      this.element.textContent = title;

      this.element.className = 'debug-tool-button';
      this.element.addEventListener('click', this.onClick.bind(this));

      parent.appendChild(this.element);

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
      this.titleDiv =
          /** @type {!HTMLElement} */ (document.createElement('h2'));
      this.titleDiv.textContent = title;

      let panel = /** @type {!HTMLElement} */ (document.createElement('div'));
      panel.className = 'debug-tool-panel';

      parent.appendChild(this.titleDiv);
      parent.appendChild(panel);
      this.content = panel;
    }

    show() {
      this.titleDiv.removeAttribute('hidden');
      this.content.removeAttribute('hidden');
    }

    clearContent() {
      let range = document.createRange();
      range.selectNodeContents(this.content);
      range.deleteContents();
    }

    hide() {
      this.titleDiv.setAttribute('hidden', true);
      this.content.setAttribute('hidden', true);
    }
  }

  class DebuggerUI {
    constructor() {
      this.debuggerVisible_ = false;
    }

    showDebugUI() {
      if (this.debuggerVisible_)
        return;
      this.refreshScreensPanel();
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

    createScreensPanel(parent) {
      let panel = new ToolPanel(this.debuggerOverlay_, 'Screens');
      // List of screens will be created later, as not all screens
      // might be registered at this point.
      this.screensPanel = panel;
    }

    createStatesPanel(parent) {
      let panel = new ToolPanel(this.debuggerOverlay_, 'Screen States');
      // List of states is rebuilt every time to reflect current screen.
      this.statesPanel = panel;
    }

    switchToScreen(screen) {
      var data = {};
      if (screen.data) {
        data = screen.data;
      }
      /** @suppress {visibility} */
      cr.ui.Oobe.instance_.showScreen({id: screen.id, data: data});
      this.lastScreenState = undefined;
    }

    triggerScreenState(stateId, toggleFn) {
      this.lastScreenState = stateId;
      /** @suppress {visibility} */
      let displayManager = cr.ui.Oobe.instance_;
      toggleFn(displayManager.currentScreen);
    }

    createScreensList() {
      this.screenMap = {};
      // Ordering of the screens.
      KNOWN_SCREENS.forEach((screen, index) => {
        screen.index = index;
        this.screenMap[screen.id] = screen;
      });
      this.knownScreens = [];
      this.screenButtons = {};
      /** @suppress {visibility} */
      for (var id of cr.ui.Oobe.instance_.screens_) {
        if (id in this.screenMap) {
          this.knownScreens.push(this.screenMap[id]);
        } else {
          console.error('### Screen not registered in debug overlay ' + id);
          let unknownScreen = {
            id: id,
            kind: ScreenKind.UNKNOWN,
            suffix: '???',
            index: this.knownScreens.length + 1000,
          };
          this.knownScreens.push(unknownScreen);
          this.screenMap[id] = unknownScreen;
        }
      }
      this.knownScreens = this.knownScreens.sort((a, b) => a.index - b.index);
      let content = this.screensPanel.content;
      this.knownScreens.forEach((screen) => {
        var name = screen.id;
        if (screen.suffix) {
          name = name + ' (' + screen.suffix + ')';
        }
        var button = new DebugButton(
            content, name, this.switchToScreen.bind(this, screen));
        button.element.classList.add('debug-button-' + screen.kind);
        this.screenButtons[screen.id] = button;
      });
    }

    refreshScreensPanel() {
      if (this.knownScreens === undefined) {
        this.createScreensList();
      }
      /** @suppress {visibility} */
      let displayManager = cr.ui.Oobe.instance_;
      if (this.lastScreenId) {
        this.screenButtons[this.lastScreenId].element.classList.remove(
            'debug-button-selected');
      }
      if (displayManager.currentScreen) {
        this.lastScreenId = displayManager.currentScreen.id;
        this.screenButtons[this.lastScreenId].element.classList.add(
            'debug-button-selected');
      }

      var states = [];

      if (this.screenMap[this.lastScreenId].states) {
        states = states.concat(this.screenMap[this.lastScreenId].states);
      }

      this.statesPanel.clearContent();
      states.forEach(state => {
        var button = new DebugButton(
            this.statesPanel.content, state.id,
            this.triggerScreenState.bind(this, state.id, state.trigger));
      });

      if (states.length > 0)
        this.statesPanel.show();
      else
        this.statesPanel.hide();
    }

    createCssStyle(name, styleSpec) {
      var style = document.createElement('style');
      style.type = 'text/css';
      style.innerHTML = '.' + name + ' {' + styleSpec + '}';
      document.getElementsByTagName('head')[0].appendChild(style);
    }

    register(element) {
      // Create CSS styles
      {
        this.createCssStyle('debugger-button', DEBUG_BUTTON_STYLE);
        this.createCssStyle('debugger-overlay', DEBUG_OVERLAY_STYLE);
        this.createCssStyle('debug-tool-panel', TOOL_PANEL_STYLE);
        this.createCssStyle('debug-tool-button', TOOL_BUTTON_STYLE);
        this.createCssStyle(
            'debug-button-selected', TOOL_BUTTON_SELECTED_STYLE);
        this.createCssStyle('debug-button-normal', SCREEN_BUTTON_STYLE_NORMAL);
        this.createCssStyle('debug-button-error', SCREEN_BUTTON_STYLE_ERROR);
        this.createCssStyle('debug-button-other', SCREEN_BUTTON_STYLE_OTHER);
        this.createCssStyle(
            'debug-button-unknown', SCREEN_BUTTON_STYLE_UNKNOWN);
      }
      {
        // Create UI Debugger button
        let button =
            /** @type {!HTMLElement} */ (document.createElement('div'));
        button.className = 'debugger-button';
        button.textContent = 'Debug';
        button.addEventListener('click', this.toggleDebugUI.bind(this));

        this.debuggerButton_ = button;
      }
      {
        // Create base debugger panel.
        let overlay =
            /** @type {!HTMLElement} */ (document.createElement('div'));
        overlay.className = 'debugger-overlay';
        overlay.setAttribute('hidden', true);
        this.debuggerOverlay_ = overlay;
      }
      this.createLanguagePanel(this.debuggerOverlay_);
      this.createScreensPanel(this.debuggerOverlay_);
      this.createStatesPanel(this.debuggerOverlay_);

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
