// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Color picker used by <input type='color' />
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
      } else if (colorStringOrFormat.startsWith('rgb')) {
        // Ex. 'rgb(255, 255, 255)' => [255,255,255]
        colorStringOrFormat = colorStringOrFormat.replace(/\s+/g, '');
        [this.rValue_, this.gValue_, this.bValue_] =
            colorStringOrFormat.substring(4, colorStringOrFormat.length - 1)
            .split(',').map(Number);
      }
      // TODO(crbug.com/982088): Add support for HSL
    } else {
      switch(colorStringOrFormat) {
        case ColorFormat.HEX:
          this.hexValue_ = colorValues[0].toLowerCase();
          break;
        case ColorFormat.RGB:
          [this.rValue_, this.gValue_, this.bValue_] = colorValues.map(Number);
          break;
        // TODO(crbug.com/982088): Add support for HSL
      }
    }
  }

  /**
   * @param {!Color} other
   */
  equals(other) {
    return (this.hexValue === other.hexValue);
  }

  /**
   * @returns {string}
   */
  get hexValue() {
    this.computeHexValue_();
    return this.hexValue_;
  }

  computeHexValue_() {
    if (this.hexValue_ !== undefined) {
      // Already computed.
    } else if (this.rValue_ !== undefined) {
      this.hexValue_ =
          Color.rgbToHex(this.rValue_, this.gValue_, this.bValue_);
    }
    // TODO(crbug.com/982088): Add support for HSL
  }

  asHex() {
    return '#' + this.hexValue;
  }

  /**
   * @returns {number} between 0 and 255
   */
  get rValue() {
    this.computeRGBValues_();
    return this.rValue_;
  }

  /**
   * @returns {number} between 0 and 255
   */
  get gValue() {
    this.computeRGBValues_();
    return this.gValue_;
  }

  /**
   * @returns {number} between 0 and 255
   */
  get bValue() {
    this.computeRGBValues_();
    return this.bValue_;
  }

  computeRGBValues_() {
    if (this.rValue_ !== undefined) {
      // Already computed.
    } else if (this.hexValue_ !== undefined) {
      [this.rValue_, this.gValue_, this.bValue_] =
          Color.hexToRGB(this.hexValue_);
    }
    // TODO(crbug.com/982088): Add support for HSL
  }

  rgbValues() {
    return [this.rValue, this.gValue, this.bValue];
  }

  asRGB() {
    return 'rgb(' + this.rgbValues().join() + ')';
  }

  /**
   * @param {string} hexValue
   * @returns {number[]}
   */
  static hexToRGB(hexValue) {
    // Ex. 'ffffff' => '[255,255,255]'
    const colorValue = parseInt(hexValue, 16);
    return [(colorValue >> 16) & 255, (colorValue >> 8) & 255, colorValue & 255];
  }

  /**
   * @param {...number} rgbValues
   * @returns {string}
   */
  static rgbToHex(...rgbValues) {
    // Ex. '[255,255,255]' => 'ffffff'
    return rgbValues.reduce((cumulativeHexValue, rgbValue) => {
      let hexValue = Number(rgbValue).toString(16);
      if(hexValue.length == 1) {
        hexValue = '0' + hexValue;
      }
      return (cumulativeHexValue + hexValue);
    }, '');
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
    const newColor = event.detail.color;
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

    this.hexValueContainer_ = new ColorValueContainer(ColorChannel.HEX,
                                                      initialColor);
    this.rgbValueContainer_ = new ColorValueContainer(ColorFormat.RGB,
                                                      initialColor);
    this.colorValueContainers_ = [
      this.hexValueContainer_,
      this.rgbValueContainer_,
      // TODO(crbug.com/982088): Add support for HSL
    ];
    this.formatToggler_ = new FormatToggler(ColorFormat.RGB);
    this.append(...this.colorValueContainers_, this.formatToggler_);

    this.hexValueContainer_.hide();
    this.rgbValueContainer_.show();

    this.formatToggler_
    .addEventListener('format-change', this.onFormatChange_);

    this.addEventListener('manual-color-change', this.onManualColorChange_);
  }

  /**
   * @param {!Event} event
   */
  onFormatChange_ = (event) => {
    const newColorFormat = event.detail.colorFormat;
    this.colorValueContainers_.forEach((colorValueContainer) => {
      if (colorValueContainer.colorFormat === newColorFormat) {
        colorValueContainer.show();
      } else {
        colorValueContainer.hide();
      }
    });
  }

  /**
   * @param {!Event} event
   */
  onManualColorChange_ = (event) => {
    const newColor = event.detail.color;
    this.colorValueContainers_.forEach((colorValueContainer) =>
        colorValueContainer.color = newColor);
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
      const hexValueContainer = new ChannelValueContainer(ColorChannel.HEX,
                                                          initialColor);
      this.channelValueContainers_.push(hexValueContainer);
    } else if (this.colorFormat_ === ColorFormat.RGB) {
      const rValueContainer = new ChannelValueContainer(ColorChannel.R,
                                                        initialColor);
      const gValueContainer = new ChannelValueContainer(ColorChannel.G,
                                                        initialColor);
      const bValueContainer = new ChannelValueContainer(ColorChannel.B,
                                                        initialColor);
      this.channelValueContainers_.push(rValueContainer,
                                        gValueContainer,
                                        bValueContainer);
    }
    // TODO(crbug.com/982088): Add support for HSL
    this.append(...this.channelValueContainers_);

    this.channelValueContainers_.forEach((channelValueContainer) =>
        channelValueContainer.addEventListener('input',
            this.onChannelValueChange_));
  }

  get colorFormat() {
    return this.colorFormat_;
  }

  get color() {
    return new Color(this.colorFormat_,
        ...this.channelValueContainers_.map((channelValueContainer) =>
            channelValueContainer.channelValue));
  }

  /**
   * @param {!Color} color
   */
  set color(color) {
    this.channelValueContainers_.forEach((channelValueContainer) =>
        channelValueContainer.setValue(color));
  }

  show() {
    return this.classList.remove('hidden-color-value-container');
  }

  hide() {
    return this.classList.add('hidden-color-value-container');
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
      case ColorChannel.R:
      case ColorChannel.G:
      case ColorChannel.B:
        this.setAttribute('maxLength', '3');
        break;
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
        if (this.channelValue_ !== color.hexValue) {
          this.channelValue_ = color.hexValue;
          this.value = '#' + this.channelValue_;
        }
        break;
      case ColorChannel.R:
        if (this.channelValue_ !== color.rValue) {
          this.channelValue_ = color.rValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.G:
        if (this.channelValue_ !== color.gValue) {
          this.channelValue_ = color.gValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.B:
        if (this.channelValue_ !== color.bValue) {
          this.channelValue_ = color.bValue;
          this.value = this.channelValue_;
        }
        break;
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
            value = value.substr(1).toLowerCase();
            if (value.match(/^[0-9a-f]+$/)) {
              // Ex. 'ffffff' => this.channelValue_ == 'ffffff'
              // Ex. 'ff' => this.channelValue_ == '0000ff'
              this.channelValue_ = ('000000' + value).slice(-6);
            }
          }
          break;
        case ColorChannel.R:
        case ColorChannel.G:
        case ColorChannel.B:
          if (value.match(/^\d+$/) && (0 <= value) && (value <= 255)) {
            this.channelValue_ = Number(value);
          }
          break;
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
  /**
   * @param {!ColorFormat} initialColorFormat
   */
  constructor(initialColorFormat) {
    super();

    this.currentColorFormat_ = initialColorFormat;
    this.hexFormatLabel_ = new FormatLabel(ColorFormat.HEX);
    this.rgbFormatLabel_ = new FormatLabel(ColorFormat.RGB);
    this.colorFormatLabels_ = [
      this.hexFormatLabel_,
      this.rgbFormatLabel_,
      // TODO(crbug.com/982088): Add support for HSL
    ];
    this.adjustFormatLabelVisibility();

    this.upDownIcon_ = document.createElement('span');
    this.upDownIcon_.innerHTML =
        '<svg width="6" height="8" viewBox="0 0 6 8" fill="none" ' +
        'xmlns="http://www.w3.org/2000/svg"><path d="M1.18359 ' +
        '3.18359L0.617188 2.61719L3 0.234375L5.38281 2.61719L4.81641 ' +
        '3.18359L3 1.36719L1.18359 3.18359ZM4.81641 4.81641L5.38281 ' +
        '5.38281L3 7.76562L0.617188 5.38281L1.18359 4.81641L3 ' +
        '6.63281L4.81641 4.81641Z" fill="black"/></svg>';

    this.append(...this.colorFormatLabels_, this.upDownIcon_);

    this.addEventListener('click', this.onClick_);
    this.addEventListener('mousedown', (event) => event.preventDefault());
  }

  adjustFormatLabelVisibility() {
    this.colorFormatLabels_.forEach((colorFormatLabel) => {
      if (colorFormatLabel.colorFormat === this.currentColorFormat_) {
        colorFormatLabel.show();
      } else {
        colorFormatLabel.hide();
      }
    });
  }

  onClick_ = () => {
    if (this.currentColorFormat_ == ColorFormat.HEX) {
      this.currentColorFormat_ = ColorFormat.RGB;
    } else if (this.currentColorFormat_ == ColorFormat.RGB) {
      this.currentColorFormat_ = ColorFormat.HEX;
    }
    // TODO(crbug.com/982088): Add support for HSL
    this.adjustFormatLabelVisibility();

    this.dispatchEvent(new CustomEvent('format-change', {
      detail: {
        colorFormat: this.currentColorFormat_
      }
    }));
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

    this.colorFormat_ = colorFormat;
    if (colorFormat === ColorFormat.HEX) {
      this.hexChannelLabel_ = new ChannelLabel(ColorChannel.HEX);
      this.append(this.hexChannelLabel_);
    } else {
      this.rChannelLabel_ = new ChannelLabel(ColorChannel.R);
      this.gChannelLabel_ = new ChannelLabel(ColorChannel.G);
      this.bChannelLabel_ = new ChannelLabel(ColorChannel.B);
      this.append(this.rChannelLabel_,
                  this.gChannelLabel_,
                  this.bChannelLabel_);
    }
    // TODO(crbug.com/982088): Add support for HSL
  }

  get colorFormat() {
    return this.colorFormat_;
  }

  show() {
    return this.classList.remove('hidden-format-label');
  }

  hide() {
    return this.classList.add('hidden-format-label');
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
    } else if (colorChannel === ColorChannel.R) {
      this.textContent = 'R';
    } else if (colorChannel === ColorChannel.G) {
      this.textContent = 'G';
    } else if (colorChannel === ColorChannel.B) {
      this.textContent = 'B';
    }
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