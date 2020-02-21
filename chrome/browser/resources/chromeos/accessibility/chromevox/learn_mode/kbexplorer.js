// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script for ChromeOS keyboard explorer.
 *
 */

goog.provide('KbExplorer');

goog.require('BrailleCommandData');
goog.require('GestureCommandData');
goog.require('Spannable');
goog.require('AbstractTts');
goog.require('BrailleKeyCommand');
goog.require('ChromeVoxKbHandler');
goog.require('CommandStore');
goog.require('KeyMap');
goog.require('KeyUtil');
goog.require('LibLouis');

/**
 * Class to manage the keyboard explorer.
 */
KbExplorer = class {
  constructor() {}

  /**
   * Initialize keyboard explorer.
   */
  static init() {
    // Export global objects from the background page context into this one.
    window.backgroundWindow = chrome.extension.getBackgroundPage();
    window.ChromeVox = window.backgroundWindow['ChromeVox'];

    window.backgroundWindow.addEventListener(
        'keydown', KbExplorer.onKeyDown, true);
    window.backgroundWindow.addEventListener('keyup', KbExplorer.onKeyUp, true);
    window.backgroundWindow.addEventListener(
        'keypress', KbExplorer.onKeyPress, true);
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(
        KbExplorer.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.addListener(
        KbExplorer.onAccessibilityGesture);
    chrome.accessibilityPrivate.setKeyboardListener(true, true);
    window.backgroundWindow['BrailleCommandHandler']['setEnabled'](false);
    window.backgroundWindow['GestureCommandHandler']['setEnabled'](false);

    ChromeVoxKbHandler.handlerKeyMap = KeyMap.fromDefaults();

    /** @type {LibLouis.Translator} */
    KbExplorer.currentBrailleTranslator_ =
        window
            .backgroundWindow['BrailleBackground']['getInstance']()['getTranslatorManager']()['getDefaultTranslator']();

    ChromeVoxKbHandler.commandHandler = KbExplorer.onCommand;
    $('instruction').focus();

    KbExplorer.output(Msgs.getMsg('learn_mode_intro'));
  }

  /**
   * Handles keydown events by speaking the human understandable name of the
   * key.
   * @param {Event} evt key event.
   * @return {boolean} True if the default action should be performed.
   */
  static onKeyDown(evt) {
    if (KbExplorer.keydownWithoutKeyupEvents_.size == 0) {
      ChromeVox.tts.stop();
    }
    KbExplorer.keydownWithoutKeyupEvents_.add(evt.keyCode);
    ChromeVox.tts.speak(
        KeyUtil.getReadableNameForKeyCode(evt.keyCode),
        window.backgroundWindow.QueueMode.QUEUE,
        AbstractTts.PERSONALITY_ANNOTATION);

    // Allow Ctrl+W or escape to be handled.
    if ((evt.key == 'w' && evt.ctrlKey) || evt.key == 'Escape') {
      KbExplorer.close_();
      return true;
    }

    ChromeVoxKbHandler.basicKeyDownActionsListener(evt);
    KbExplorer.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
    return false;
  }

  /**
   * Handles keyup events.
   * @param {Event} evt key event.
   */
  static onKeyUp(evt) {
    KbExplorer.keydownWithoutKeyupEvents_.delete(evt.keyCode);
    KbExplorer.maybeClose_();
    KbExplorer.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  /**
   * Handles keypress events.
   * @param {Event} evt key event.
   */
  static onKeyPress(evt) {
    KbExplorer.clearRange();
    evt.preventDefault();
    evt.stopPropagation();
  }

  /**
   * @param {BrailleKeyEvent} evt The key event.
   */
  static onBrailleKeyEvent(evt) {
    KbExplorer.maybeClose_();
    let msgid;
    const msgArgs = [];
    let text;
    switch (evt.command) {
      case BrailleKeyCommand.PAN_LEFT:
        msgid = 'braille_pan_left';
        break;
      case BrailleKeyCommand.PAN_RIGHT:
        msgid = 'braille_pan_right';
        break;
      case BrailleKeyCommand.LINE_UP:
        msgid = 'braille_line_up';
        break;
      case BrailleKeyCommand.LINE_DOWN:
        msgid = 'braille_line_down';
        break;
      case BrailleKeyCommand.TOP:
        msgid = 'braille_top';
        break;
      case BrailleKeyCommand.BOTTOM:
        msgid = 'braille_bottom';
        break;
      case BrailleKeyCommand.ROUTING:
      case BrailleKeyCommand.SECONDARY_ROUTING:
        msgid = 'braille_routing';
        msgArgs.push(/** @type {number} */ (evt.displayPosition + 1));
        break;
      case BrailleKeyCommand.CHORD:
        const dots = evt.brailleDots;
        if (!dots) {
          return;
        }

        // First, check for the dots mapping to a key code.
        const keyCode = BrailleKeyEvent.brailleChordsToStandardKeyCode[dots];
        if (keyCode) {
          text = keyCode;
          break;
        }

        // Next, check for the modifier mappings.
        const mods = BrailleKeyEvent.brailleDotsToModifiers[dots];
        if (mods) {
          const outputs = [];
          for (const mod in mods) {
            if (mod == 'ctrlKey') {
              outputs.push('control');
            } else if (mod == 'altKey') {
              outputs.push('alt');
            } else if (mod == 'shiftKey') {
              outputs.push('shift');
            }
          }

          text = outputs.join(' ');
          break;
        }

        const command = BrailleCommandData.getCommand(dots);
        if (command && KbExplorer.onCommand(command)) {
          return;
        }
        text = BrailleCommandData.makeShortcutText(dots, true);
        break;
      case BrailleKeyCommand.DOTS: {
        const dots = evt.brailleDots;
        if (!dots) {
          return;
        }
        const cells = new ArrayBuffer(1);
        const view = new Uint8Array(cells);
        view[0] = dots;
        KbExplorer.currentBrailleTranslator_.backTranslate(
            cells, function(res) {
              KbExplorer.output(res);
            }.bind(this));
      }
        return;
      case BrailleKeyCommand.STANDARD_KEY:
        break;
    }
    if (msgid) {
      text = Msgs.getMsg(msgid, msgArgs);
    }
    KbExplorer.output(text || evt.command);
    KbExplorer.clearRange();
  }

  /**
   * Handles accessibility gestures from the touch screen.
   * @param {string} gesture The gesture to handle, based on the
   *     ax::mojom::Gesture enum defined in ui/accessibility/ax_enums.mojom
   */
  static onAccessibilityGesture(gesture) {
    KbExplorer.maybeClose_();
    const gestureData = GestureCommandData.GESTURE_COMMAND_MAP[gesture];
    if (gestureData) {
      KbExplorer.onCommand(gestureData.command);
    }
  }

  /**
   * Queues up command description.
   * @param {string} command
   * @return {boolean|undefined} True if command existed and was handled.
   */
  static onCommand(command) {
    const msg = CommandStore.messageForCommand(command);
    if (msg) {
      const commandText = Msgs.getMsg(msg);
      KbExplorer.output(commandText);
      KbExplorer.clearRange();
      return true;
    }
  }

  /**
   * @param {string} text
   * @param {string=} opt_braille If different from text.
   */
  static output(text, opt_braille) {
    ChromeVox.tts.speak(text, window.backgroundWindow.QueueMode.QUEUE);
    ChromeVox.braille.write({text: new Spannable(opt_braille || text)});
  }

  /** Clears ChromeVox range. */
  static clearRange() {
    chrome.extension
        .getBackgroundPage()['ChromeVoxState']['instance']['setCurrentRange'](
            null);
  }

  /** @private */
  static resetListeners_() {
    window.backgroundWindow.removeEventListener(
        'keydown', KbExplorer.onKeyDown, true);
    window.backgroundWindow.removeEventListener(
        'keyup', KbExplorer.onKeyUp, true);
    window.backgroundWindow.removeEventListener(
        'keypress', KbExplorer.onKeyPress, true);
    chrome.brailleDisplayPrivate.onKeyEvent.removeListener(
        KbExplorer.onBrailleKeyEvent);
    chrome.accessibilityPrivate.onAccessibilityGesture.removeListener(
        KbExplorer.onAccessibilityGesture);
    chrome.accessibilityPrivate.setKeyboardListener(true, false);
    window.backgroundWindow['BrailleCommandHandler']['setEnabled'](true);
    window.backgroundWindow['GestureCommandHandler']['setEnabled'](true);
  }

  /** @private */
  static maybeClose_() {
    // Reset listeners and close this page if we somehow move outside of the
    // explorer window.
    chrome.windows.getLastFocused({populate: true}, (focusedWindow) => {
      if (focusedWindow && focusedWindow.focused &&
          focusedWindow.tabs.find((tab) => {
            return tab.url == location.href;
          })) {
        return;
      }

      KbExplorer.close_();
    });
  }

  /** @private */
  static close_() {
    KbExplorer.output(Msgs.getMsg('learn_mode_outtro'));
    KbExplorer.resetListeners_();
    window.close();
  }
};

/**
 * Tracks all keydown events (keyed by key code) for which keyup events have
 * yet to be received.
 * @type {!Set<number>}
 */
KbExplorer.keydownWithoutKeyupEvents_ = new Set();
