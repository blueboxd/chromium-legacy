// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class handles interactions with combo boxes.
 * TODO(anastasi): Add a test for this class.
 */
class ComboBoxNode extends NodeWrapper {
  /** @override */
  get actions() {
    return [SwitchAccessMenuAction.INCREMENT, SwitchAccessMenuAction.DECREMENT];
  }

  /** @override */
  doDefaultAction() {
    this.performAction(SwitchAccessMenuAction.INCREMENT);
  }

  /** @override */
  onFocus() {
    super.onFocus();
    this.automationNode.focus();
  }

  /** @override */
  performAction(action) {
    // The box of options that typically pops up with combo boxes is not
    // currently represented in the automation tree, so we work around that by
    // selecting a value without opening the pop-up, using the up and down
    // arrows.
    switch (action) {
      case SwitchAccessMenuAction.DECREMENT:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.UP_ARROW);
        return SAConstants.ActionResponse.REMAIN_OPEN;
      case SwitchAccessMenuAction.INCREMENT:
        EventHelper.simulateKeyPress(EventHelper.KeyCode.DOWN_ARROW);
        return SAConstants.ActionResponse.REMAIN_OPEN;
    }
    return SAConstants.ActionResponse.NO_ACTION_TAKEN;
  }
}
