// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Color picker used by <input type='color' />
 */

/**
 * @param {!Object} args
 */
function initializeColorPicker() {
  if (global.params.selectedColor === undefined) {
    global.params.selectedColor = DefaultColor;
  }
  const colorPicker = new ColorPicker(new Color(global.params.selectedColor));
  main.append(colorPicker);
  const width = colorPicker.offsetWidth;
  const height = colorPicker.offsetHeight;
  resizeWindow(width, height);
}

/**
 * @param {!Object} args
 * @return {?string} An error message, or null if the argument has no errors.
 */
function validateColorPickerArguments(args) {
  if (args.shouldShowColorSuggestionPicker)
    return 'Should be showing the color suggestion picker.'
  if (!args.selectedColor)
    return 'No selectedColor.';
  return null;
}

/**
 * Supported color channels.
 * @enum {number}
 */
const ColorChannel = {
  UNDEFINED: 0,
  HEX: 1,
  R: 2,
  G: 3,
  B: 4,
  H: 5,
  S: 6,
  L: 7,
};

/**
 * Supported color formats.
 * @enum {number}
 */
const ColorFormat = {
  UNDEFINED: 0,
  HEX: 1,
  RGB: 2,
  HSL: 3,
};

/**
 * Color: Helper class to get color values in different color formats.
 */
class Color {
  /**
   * @param {string|!ColorFormat} colorStringOrFormat
   * @param  {...number} colorValues ignored if colorStringOrFormat is a string
   */
  constructor(colorStringOrFormat, ...colorValues) {
    if (typeof colorStringOrFormat === 'string') {
      colorStringOrFormat = colorStringOrFormat.toLowerCase();
      if (colorStringOrFormat.startsWith('#')) {
        this.hexValue_ = colorStringOrFormat.substr(1);
      }
      // TODO(crbug.com/982087): Add support for RGB
      // TODO(crbug.com/982088): Add support for HSL
    } else {
      switch(colorStringOrFormat) {
        case ColorFormat.HEX:
          this.hexValue_ = colorValues[0];
          break;
        // TODO(crbug.com/982087): Add support for RGB
        // TODO(crbug.com/982088): Add support for HSL
      }
    }
  }

  /**
   * @param {!Color} other
   */
  equals(other) {
    return (this.hexValue_ === other.hexValue_);
  }

  get hexValue() {
    return this.hexValue_;
  }

  asHex() {
    return '#' + this.hexValue_;
  }
}

/**
 * ColorPicker: Custom element providing a color picker implementation.
 *              A color picker is comprised of three main parts: a visual color
 *              picker to allow visual selection of colors, a manual color
 *              picker to allow numeric selection of colors, and submission
 *              controls to save/discard new color selections.
 */
class ColorPicker extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.selectedColor_ = initialColor;

    this.visualColorPicker_ = new VisualColorPicker(initialColor);
    this.manualColorPicker_ = new ManualColorPicker(initialColor);
    this.submissionControls_ =
        new SubmissionControls(this.onSubmitButtonClick_,
                               this.onCancelButtonClick_);
    this.append(this.visualColorPicker_,
                this.manualColorPicker_,
                this.submissionControls_);

    this.manualColorPicker_
        .addEventListener('manual-color-change', this.onManualColorChange_);
  }

  get selectedColor() {
    return this.selectedColor_;
  }

  /**
   * @param {!Color} newColor
   */
  set selectedColor(newColor) {
    this.selectedColor_ = newColor;
  }

  /**
   * @param {!Event} event
   */
  onManualColorChange_ = (event) => {
    var newColor = event.detail.color;
    if (!this.selectedColor.equals(newColor)) {
      this.selectedColor = newColor;
      this.visualColorPicker_.setSelectedColor(newColor);
    }
  }

  onSubmitButtonClick_ = () => {
    const selectedValue = this.selectedColor_.asHex();
    window.setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, selectedValue);
    }, 100);
  }

  onCancelButtonClick_ = () => {
    window.pagePopupController.closePopup();
  }
}
window.customElements.define('color-picker', ColorPicker);

/**
 * VisualColorPicker: Provides functionality to see the selected color and
 *                    select a different color visually.
 * TODO(crbug.com/983311): Allow colors to be selected from within the visual
 *                         color picker.
 */
class VisualColorPicker extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.setSelectedColor(initialColor);
  }

  /**
   * @param {!Color} newColor
   */
  setSelectedColor(newColor) {
    this.style.backgroundColor = newColor.asHex();
  }
}
window.customElements.define('visual-color-picker', VisualColorPicker);

/**
 * ManualColorPicker: Provides functionality to change the selected color by
 *                    manipulating its numeric values.
 */
class ManualColorPicker extends HTMLElement {
  /**
   * @param {!Color} initialColor
   */
  constructor(initialColor) {
    super();

    this.colorValueContainers_ = [
      new ColorValueContainer(ColorFormat.HEX,
                              initialColor)
      // TODO(crbug.com/982087): Add support for RGB
      // TODO(crbug.com/982088): Add support for HSL
    ];
    this.formatToggler_ = new FormatToggler();
    this.append(...this.colorValueContainers_, this.formatToggler_);
  }
}
window.customElements.define('manual-color-picker', ManualColorPicker);

/**
 * ColorValueContainer: Maintains a set of channel values that make up a given
 *                      color format, and tracks value changes.
 */
class ColorValueContainer extends HTMLElement {
  /**
   * @param {!ColorFormat} colorFormat
   * @param {!Color} initialColor
   */
  constructor(colorFormat, initialColor) {
    super();

    this.colorFormat_ = colorFormat;
    this.channelValueContainers_ = [];
    if (this.colorFormat_ === ColorFormat.HEX) {
      const hexValueContainer =
          new ChannelValueContainer(ColorChannel.HEX,
                                    initialColor);
      this.channelValueContainers_.push(hexValueContainer);
    }
    this.append(...this.channelValueContainers_);

    this.channelValueContainers_.forEach((channelValueContainer) =>
        channelValueContainer.addEventListener('input',
            this.onChannelValueChange_));
  }

  get color() {
    return new Color(this.colorFormat_,
        ...this.channelValueContainers_.map((channelValueContainer) =>
            channelValueContainer.channelValue));
  }

  onChannelValueChange_ = () => {
    this.dispatchEvent(new CustomEvent('manual-color-change', {
      bubbles: true,
      detail: {
        color: this.color
      }
    }));
  }
}
window.customElements.define('color-value-container', ColorValueContainer);

/**
 * ChannelValueContainer: Maintains and displays the numeric value
 *                        for a given color channel.
 */
class ChannelValueContainer extends HTMLInputElement {
  /**
   * @param {!ColorChannel} colorChannel
   * @param {!Color} initialColor
   */
  constructor(colorChannel, initialColor) {
    super();

    this.setAttribute('type', 'text');
    this.colorChannel_ = colorChannel;
    switch(colorChannel) {
      case ColorChannel.HEX:
        this.setAttribute('maxlength', '7');
        break;
      // TODO(crbug.com/982087): Add support for RGB
      // TODO(crbug.com/982088): Add support for HSL
    }
    this.setValue(initialColor);

    this.addEventListener('input', this.onValueChange_);
  }

  get channelValue() {
    return this.channelValue_;
  }

  /**
   * @param {!Color} color
   */
  setValue(color) {
    switch(this.colorChannel_) {
      case ColorChannel.HEX:
        this.channelValue_ = color.hexValue;
        this.value = '#' + this.channelValue_;
        break;
      // TODO(crbug.com/982087): Add support for RGB
      // TODO(crbug.com/982088): Add support for HSL
    }
  }

  onValueChange_ = () => {
    // Set this.channelValue_ based on the element's new value.
    let value = this.value;
    if (value) {
      switch(this.colorChannel_) {
        case ColorChannel.HEX:
          if (value.startsWith('#')) {
            value = value.substr(1);
            if (value.match(/^[0-9a-fA-F]+$/)) {
              // Ex. 'FFFFFF' => this.channelValue_ == 'FFFFFF'
              // Ex. 'FF' => this.channelValue_ == '0000FF'
              this.channelValue_ = ('000000' + value).slice(-6);
            }
          }
          break;
        // TODO(crbug.com/982087): Add support for RGB
        // TODO(crbug.com/982088): Add support for HSL
      }
    }
  }
}
window.customElements.define('channel-value-container',
                             ChannelValueContainer,
                             { extends: 'input' });

/**
 * FormatToggler: Button that powers switching between different color formats.
 */
class FormatToggler extends HTMLElement {
  constructor() {
    super();

    this.colorFormatLabels_ = [
      new FormatLabel(ColorFormat.HEX)
      // TODO(crbug.com/982087): Add support for RGB
      // TODO(crbug.com/982088): Add support for HSL
    ];
    this.append(...this.colorFormatLabels_);
  }
}
window.customElements.define('format-toggler', FormatToggler);

/**
 * FormatLabel: Label for a given color format.
 */
class FormatLabel extends HTMLElement {
  /**
   * @param {!ColorFormat} colorFormat
   */
  constructor(colorFormat) {
    super();

    if (colorFormat === ColorFormat.HEX) {
      this.hexChannelLabel_ = new ChannelLabel(ColorChannel.HEX);
      this.append(this.hexChannelLabel_);
    }
    // TODO(crbug.com/982087): Add support for RGB
    // TODO(crbug.com/982088): Add support for HSL
  }
}
window.customElements.define('format-label', FormatLabel);

/**
 * ChannelLabel: Label for a color channel, to be used within a FormatLabel.
 */
class ChannelLabel extends HTMLElement {
  /**
   * @param {!ColorChannel} colorChannel
   */
  constructor(colorChannel) {
    super();

    if (colorChannel === ColorChannel.HEX) {
      this.textContent = 'HEX';
    }
    // TODO(crbug.com/982087): Add support for RGB
    // TODO(crbug.com/982088): Add support for HSL
  }
}
window.customElements.define('channel-label', ChannelLabel);

/**
 * SubmissionControls: Provides functionality to submit or discard a change.
 */
class SubmissionControls extends HTMLElement {
  /**
   * @param {function} submitCallback executed if the submit button is clicked
   * @param {function} cancelCallback executed if the cancel button is clicked
   */
  constructor(submitCallback, cancelCallback) {
    super();

    const padding = document.createElement('span');
    padding.setAttribute('id', 'submission-controls-padding');
    this.append(padding);

    this.submitButton_ = new SubmissionButton(submitCallback,
        '<svg width="14" height="10" viewBox="0 0 14 10" fill="none" ' +
        'xmlns="http://www.w3.org/2000/svg"><path d="M13.3516 ' +
        '1.35156L5 9.71094L0.648438 5.35156L1.35156 4.64844L5 ' +
        '8.28906L12.6484 0.648438L13.3516 1.35156Z" fill="black"/></svg>'
    );
    this.cancelButton_ = new SubmissionButton(cancelCallback,
        '<svg width="14" height="14" viewBox="0 0 14 14" fill="none" ' +
        'xmlns="http://www.w3.org/2000/svg"><path d="M7.71094 7L13.1016 ' +
        '12.3984L12.3984 13.1016L7 7.71094L1.60156 13.1016L0.898438 ' +
        '12.3984L6.28906 7L0.898438 1.60156L1.60156 0.898438L7 ' +
        '6.28906L12.3984 0.898438L13.1016 1.60156L7.71094 7Z" ' +
        'fill="black"/></svg>'
    );
    this.append(this.submitButton_, this.cancelButton_);
  }
}
window.customElements.define('submission-controls', SubmissionControls);

/**
 * SubmissionButton: Button with a custom look that can be clicked for
 *                   a submission action.
 */
class SubmissionButton extends HTMLElement {
  /**
   * @param {function} clickCallback executed when the button is clicked
   * @param {string} htmlString custom look for the button
   */
  constructor(clickCallback, htmlString) {
    super();

    this.setAttribute('tabIndex', '0');
    this.innerHTML = htmlString;

    this.addEventListener('click', clickCallback);
  }
}
window.customElements.define('submission-button', SubmissionButton);