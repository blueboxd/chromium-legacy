// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to handle focus rings.
 */
class FocusRingManager {
  /**
   * @param {!SwitchAccessInterface} switchAccess
   * @param {!BackButtonManager} backButtonManager
   */
  constructor(switchAccess, backButtonManager) {
    /**
     * A map of all the focus rings.
     * @private {!Map<SAConstants.Focus.ID,
     *     chrome.accessibilityPrivate.FocusRingInfo>}
     */
    this.rings_ = new Map();

    /**
     * Regex pattern to verify valid colors. Checks that the first character
     * is '#', followed by between 3 and 8 valid hex characters, and no other
     * characters (ignoring case).
     */
    this.colorPattern_ = /^#[0-9A-F]{3,8}$/i;

    /**
     * Keeps track of the scope node currently being focused.
     * @private {chrome.automation.AutomationNode}
     */
    this.currentScope_;

    /**
     * A reference to the Switch Access object.
     * @private {!SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * The back button manager.
     * @private {!BackButtonManager}
     */
    this.backButtonManager_ = backButtonManager;
  }

  /** Finishes setup of focus rings once the preferences are loaded. */
  onPrefsReady() {
    // Currently all focus rings share the same color.
    // TODO(anastasi): Make the primary color a preference.
    const color = SAConstants.Focus.PRIMARY_COLOR;

    // Create each focus ring.
    this.rings_.set(SAConstants.Focus.ID.PRIMARY, {
      id: SAConstants.Focus.ID.PRIMARY,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.SOLID,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
    this.rings_.set(SAConstants.Focus.ID.SCOPE, {
      id: SAConstants.Focus.ID.SCOPE,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
    this.rings_.set(SAConstants.Focus.ID.TEXT, {
      id: SAConstants.Focus.ID.TEXT,
      rects: [],
      type: chrome.accessibilityPrivate.FocusType.DASHED,
      color: color,
      secondaryColor: SAConstants.Focus.SECONDARY_COLOR
    });
  }

  /**
   * Sets the focus ring color.
   * @param {!string} color
   */
  setColor(color) {
    if (this.colorPattern_.test(color) !== true) {
      const errorString = 'Problem setting focus ring color: color is not' +
          'a valid CSS color string.';
      throw new Error(errorString);
    }
    this.rings_.forEach((ring) => ring.color = color);
  }

  /**
   * Sets the primary and scope focus rings to be around the given nodes.
   * Saves the scope for future comparison.
   * @param {!chrome.automation.AutomationNode} primary
   * @param {!chrome.automation.AutomationNode} scope
   */
  setFocusNodes(primary, scope) {
    const focusRect = primary.location;

    // If the scope element has not changed, we want to use the previously
    // calculated rect as the current scope rect.
    let scopeRect = scope.location;
    const currentScopeRects = this.rings_.get(SAConstants.Focus.ID.SCOPE).rects;
    if (currentScopeRects.length && scope === this.currentScope_)
      scopeRect = currentScopeRects[0];
    this.currentScope_ = scope;

    if (primary === this.backButtonManager_.buttonNode()) {
      this.backButtonManager_.show(scopeRect);

      this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [];
      this.rings_.get(SAConstants.Focus.ID.SCOPE).rects = [scopeRect];
      this.updateFocusRings_();
      return;
    }
    this.backButtonManager_.hide();

    // If the current element is not the back button, the scope rect should
    // expand to contain the focus rect.
    scopeRect = RectHelper.expandToFitWithPadding(
        SAConstants.Focus.SCOPE_BUFFER, scopeRect, focusRect);

    this.rings_.get(SAConstants.Focus.ID.PRIMARY).rects = [focusRect];
    this.rings_.get(SAConstants.Focus.ID.SCOPE).rects = [scopeRect];
    this.updateFocusRings_();
  }

  /**
   * Clears the focus ring with the given ID.
   * @param {!SAConstants.Focus.ID} id
   */
  clearRing(id) {
    this.rings_.get(id).rects = [];
    this.updateFocusRings_();
  }

  /**
   * Clears all focus rings.
   */
  clearAll() {
    this.rings_.forEach((ring) => ring.rects = []);
    this.updateFocusRings_();
  }

  /**
   * Sets the indicated focus ring to highlight the given rects.
   * @param {!SAConstants.Focus.ID} id
   * @param {!Array<chrome.accessibilityPrivate.ScreenRect>} rects
   */
  setRing(id, rects) {
    this.rings_.get(id).rects = rects;
    this.updateFocusRings_();
  }

  /**
   * Updates all focus rings to reflect new location, color, style, or other
   * changes.
   */
  updateFocusRings_() {
    let focusRings = [];
    this.rings_.forEach((ring) => focusRings.push(ring));
    chrome.accessibilityPrivate.setFocusRings(focusRings);
  }
}
