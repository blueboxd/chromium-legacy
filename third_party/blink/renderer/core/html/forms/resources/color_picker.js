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
      } else if (colorStringOrFormat.startsWith('hsl')) {
        colorStringOrFormat = colorStringOrFormat.replace(/%|\s+/g, '');
        [this.hValue_, this.sValue_, this.lValue_] =
            colorStringOrFormat.substring(4, colorStringOrFormat.length - 1)
            .split(',').map(Number);
      }
    } else {
      switch(colorStringOrFormat) {
        case ColorFormat.HEX:
          this.hexValue_ = colorValues[0].toLowerCase();
          break;
        case ColorFormat.RGB:
          [this.rValue_, this.gValue_, this.bValue_] = colorValues.map(Number);
          break;
        case ColorFormat.HSL:
          [this.hValue_, this.sValue_, this.lValue_] = colorValues.map(Number);
          break;
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
    } else if (this.hValue_ !== undefined) {
      this.hexValue_ =
          Color.hslToHex(this.hValue_, this.sValue_, this.lValue_);
    }
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
    } else if (this.hValue_ !== undefined) {
      [this.rValue_, this.gValue_, this.bValue_] =
          Color.hslToRGB(this.hValue_, this.sValue_, this.lValue_)
          .map(Math.round);
    }
  }

  rgbValues() {
    return [this.rValue, this.gValue, this.bValue];
  }

  asRGB() {
    return 'rgb(' + this.rgbValues().join() + ')';
  }

  /**
   * @returns {number} between 0 and 359
   */
  get hValue() {
    this.computeHSLValues_();
    return this.hValue_;
  }

  /**
   * @returns {number} between 0 and 100
   */
  get sValue() {
    this.computeHSLValues_();
    return this.sValue_;
  }

  /**
   * @returns {number} between 0 and 100
   */
  get lValue() {
    this.computeHSLValues_();
    return this.lValue_;
  }

  computeHSLValues_() {
    if (this.hValue_ !== undefined) {
      // Already computed.
    } else if (this.rValue_ !== undefined) {
      [this.hValue_, this.sValue_, this.lValue_] =
          Color.rgbToHSL(this.rValue_, this.gValue_, this.bValue_)
          .map(Math.round);
    } else if (this.hexValue_ !== undefined) {
      [this.hValue_, this.sValue_, this.lValue_] =
          Color.hexToHSL(this.hexValue_).map(Math.round);
    }
  }

  hslValues() {
    return [this.hValue, this.sValue, this.lValue];
  }

  asHSL() {
    return 'hsl(' + this.hValue + ',' + this.sValue + '%,' + this.lValue + '%)';
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

  /**
   * The algorithm has been written based on the mathematical formula found at:
   * https://en.wikipedia.org/wiki/HSL_and_HSV#HSL_to_RGB.
   * @param {...number} hslValues
   * @returns {number[]}
   */
  static hslToRGB(...hslValues) {
    let [hValue, sValue, lValue] = hslValues;
    hValue /= 60;
    sValue /= 100;
    lValue /= 100;

    let rValue = lValue;
    let gValue = lValue;
    let bValue = lValue;
    let match = 0;
    if (sValue !== 0) {
      const chroma = (1 - Math.abs(2 * lValue - 1)) * sValue;
      const x = chroma * (1 - Math.abs(hValue % 2 - 1));
      match = lValue - chroma / 2;
      if ((0 <= hValue) && (hValue <= 1)) {
        rValue = chroma;
        gValue = x;
        bValue = 0;
      } else if ((1 < hValue) && (hValue <= 2)) {
        rValue = x;
        gValue = chroma;
        bValue = 0;
      } else if ((2 < hValue) && (hValue <= 3)) {
        rValue = 0;
        gValue = chroma;
        bValue = x;
      } else if ((3 < hValue) && (hValue <= 4)) {
        rValue = 0;
        gValue = x;
        bValue = chroma;
      } else if ((4 < hValue) && (hValue <= 5)) {
        rValue = x;
        gValue = 0;
        bValue = chroma;
      } else {
        // (5 < hValue) && (hValue < 6)
        rValue = chroma;
        gValue = 0;
        bValue = x;
      }
    }
    rValue = (rValue + match) * 255;
    gValue = (gValue + match) * 255;
    bValue = (bValue + match) * 255;
    return [rValue, gValue, bValue];
  }

  /**
   * The algorithm has been written based on the mathematical formula found at:
   * https://en.wikipedia.org/wiki/HSL_and_HSV#From_RGB.
   * @param {...number} rgbValues
   * @returns {number[]}
   */
  static rgbToHSL(...rgbValues) {
    const [rValue, gValue, bValue] = rgbValues.map((value) => value / 255);
    const max = Math.max(rValue, gValue, bValue);
    const min = Math.min(rValue, gValue, bValue);
    let hValue = 0;
    let sValue = 0;
    let lValue = (max + min) / 2;
    if (max !== min) {
      const diff = max - min;
      if (max === rValue) {
        hValue = ((gValue - bValue) / diff);
      } else if (max === gValue) {
        hValue = ((bValue - rValue) / diff) + 2;
      } else {
        // max === bValue
        hValue = ((rValue - gValue) / diff) + 4;
      }
      hValue *= 60;
      if (hValue < 0) {
        hValue += 360;
      }
      sValue = (diff / (1 - Math.abs(2 * lValue - 1))) * 100;
    }
    lValue *= 100;
    return [hValue, sValue, lValue];
  }

  /**
   * @param {...number} rgbValues
   * @returns {string}
   */
  static hslToHex(...hslValues) {
    return Color.rgbToHex(...Color.hslToRGB(...hslValues).map(Math.round));
  }

  /**
   * @param {string} hexValue
   * @returns {...number}
   */
  static hexToHSL(hexValue) {
    return Color.rgbToHSL(...Color.hexToRGB(hexValue));
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
    this.hslValueContainer_ = new ColorValueContainer(ColorFormat.HSL,
                                                      initialColor);
    this.colorValueContainers_ = [
      this.hexValueContainer_,
      this.rgbValueContainer_,
      this.hslValueContainer_,
    ];
    this.currentColorFormat_ = ColorFormat.RGB;
    this.adjustValueContainerVisibility_();
    this.formatToggler_ = new FormatToggler(this.currentColorFormat_);
    this.append(...this.colorValueContainers_, this.formatToggler_);

    this.formatToggler_
    .addEventListener('format-change', this.onFormatChange_);

    this.addEventListener('manual-color-change', this.onManualColorChange_);
  }

  adjustValueContainerVisibility_() {
    this.colorValueContainers_.forEach((colorValueContainer) => {
      if (colorValueContainer.colorFormat === this.currentColorFormat_) {
        colorValueContainer.show();
      } else {
        colorValueContainer.hide();
      }
    });
  }

  /**
   * @param {!Event} event
   */
  onFormatChange_ = (event) => {
    this.currentColorFormat_ = event.detail.colorFormat;
    this.adjustValueContainerVisibility_();
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
    } else if (this.colorFormat_ === ColorFormat.HSL) {
      const hValueContainer = new ChannelValueContainer(ColorChannel.H,
                                                        initialColor);
      const sValueContainer = new ChannelValueContainer(ColorChannel.S,
                                                        initialColor);
      const lValueContainer = new ChannelValueContainer(ColorChannel.L,
                                                        initialColor);
      this.channelValueContainers_.push(hValueContainer,
                                        sValueContainer,
                                        lValueContainer);
    }
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
        this.setAttribute('maxlength', '3');
        break;
      case ColorChannel.H:
        this.setAttribute('maxlength', '3');
        break;
      case ColorChannel.S:
      case ColorChannel.L:
        // up to 3 digits plus '%'
        this.setAttribute('maxlength', '4');
        break;
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
      case ColorChannel.H:
        if (this.channelValue_ !== color.hValue) {
          this.channelValue_ = color.hValue;
          this.value = this.channelValue_;
        }
        break;
      case ColorChannel.S:
        if (this.channelValue_ !== color.sValue) {
          this.channelValue_ = color.sValue;
          this.value = this.channelValue_ + '%';
        }
        break;
      case ColorChannel.L:
        if (this.channelValue_ !== color.lValue) {
          this.channelValue_ = color.lValue;
          this.value = this.channelValue_ + '%';
        }
        break;
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
        case ColorChannel.H:
            if (value.match(/^\d+$/) && (0 <= value) && (value < 360)) {
              this.channelValue_ = Number(value);
            }
            break;
        case ColorChannel.S:
        case ColorChannel.L:
          if (value.endsWith('%')) {
            value = value.substring(0, value.length - 1);
            if (value.match(/^\d+$/) && (0 <= value) && (value <= 100)) {
              this.channelValue_ = Number(value);
            }
          }
          break;
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
    this.hslFormatLabel_ = new FormatLabel(ColorFormat.HSL);
    this.colorFormatLabels_ = [
      this.hexFormatLabel_,
      this.rgbFormatLabel_,
      this.hslFormatLabel_,
    ];
    this.adjustFormatLabelVisibility_();

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

  adjustFormatLabelVisibility_() {
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
      this.currentColorFormat_ = ColorFormat.HSL;
    } else if (this.currentColorFormat_ == ColorFormat.HSL) {
      this.currentColorFormat_ = ColorFormat.HEX;
    }
    this.adjustFormatLabelVisibility_();

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
    } else if (colorFormat === ColorFormat.RGB) {
      this.rChannelLabel_ = new ChannelLabel(ColorChannel.R);
      this.gChannelLabel_ = new ChannelLabel(ColorChannel.G);
      this.bChannelLabel_ = new ChannelLabel(ColorChannel.B);
      this.append(this.rChannelLabel_,
                  this.gChannelLabel_,
                  this.bChannelLabel_);
    } else if (colorFormat === ColorFormat.HSL) {
      this.hChannelLabel_ = new ChannelLabel(ColorChannel.H);
      this.sChannelLabel_ = new ChannelLabel(ColorChannel.S);
      this.lChannelLabel_ = new ChannelLabel(ColorChannel.L);
      this.append(this.hChannelLabel_,
                  this.sChannelLabel_,
                  this.lChannelLabel_);
    }
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
    } else if (colorChannel === ColorChannel.H) {
      this.textContent = 'H';
    } else if (colorChannel === ColorChannel.S) {
      this.textContent = 'S';
    } else if (colorChannel === ColorChannel.L) {
      this.textContent = 'L';
    }
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