'use strict';
// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picker used by <input type='time' />
 */

function initializeTimePicker(config) {
  const timePicker = new TimePicker(config);
  global.picker = timePicker;
  main.append(timePicker);
  resizeWindow(timePicker.width, timePicker.height);
}

/**
 * Supported time column types.
 * @enum {number}
 */
const TimeColumnType = {
  UNDEFINED: 0,
  HOUR: 1,
  MINUTE: 2,
  SECOND: 3,
  MILLISECOND: 4,
};

/**
 * @param {string} dateTimeString
 * @return {?Day|Week|Month|Time}
 */
function parseDateTimeString(dateTimeString) {
  var time = Time.parse(dateTimeString);
  if (time)
    return time;
  return parseDateString(dateTimeString);
}

class Time {
  constructor(hour, minute, second, millisecond) {
    this.hour_ = hour;
    this.minute_ = minute;
    this.second_ = second;
    this.millisecond_ = millisecond;
  }

  next = (columnType) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        this.hour_ = (this.hour_ + 1) % Time.HOUR_VALUES;
        break;
      case TimeColumnType.MINUTE:
        this.minute_ = (this.minute_ + 1) % Time.MINUTE_VALUES;
        break;
      case TimeColumnType.SECOND:
        this.second_ = (this.second_ + 1) % Time.SECOND_VALUES;
        break;
      case TimeColumnType.MILLISECOND:
        // TODO(https://crbug.com/1008294): Use increments of 1 instead of 100 for milliseconds.
        // support 100, 200, 300... for milliseconds
        this.millisecond_ =
            (Math.round(this.millisecond_ / 100) * 100 + 100) % 1000;
        break;
    }
  };

  value = (columnType) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        return this.hour_.toString().padStart(2, '0');
      case TimeColumnType.MINUTE:
        return this.minute_.toString().padStart(2, '0');
      case TimeColumnType.SECOND:
        return this.second_.toString().padStart(2, '0');
      case TimeColumnType.MILLISECOND:
        return this.millisecond_.toString().padStart(3, '0');
    }
  };

  toString = (hasSecond, hasMillisecond) => {
    let value = `${this.value(TimeColumnType.HOUR)}:${
        this.value(TimeColumnType.MINUTE)}`;
    if (hasSecond) {
      value += `:${this.value(TimeColumnType.SECOND)}`;
    }
    if (hasMillisecond) {
      value += `.${this.value(TimeColumnType.MILLISECOND)}`;
    }
    return value;
  };

  static parse = (str) => {
    var match = Time.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    var hour = parseInt(match[1], 10);
    var minute = parseInt(match[2], 10);
    var second = 0;
    if (match[3])
      second = parseInt(match[3], 10);
    var millisecond = 0;
    if (match[4])
      millisecond = parseInt(match[4], 10);
    return new Time(hour, minute, second, millisecond);
  };

  static currentTime = () => {
    var currentDate = new Date();
    return new Time(
        currentDate.getHours(), currentDate.getMinutes(),
        currentDate.getSeconds(), currentDate.getMilliseconds());
  };

  static numberOfValues = (columnType) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        return Time.HOUR_VALUES;
      case TimeColumnType.MINUTE:
        return Time.MINUTE_VALUES;
      case TimeColumnType.SECOND:
        return Time.SECOND_VALUES;
      case TimeColumnType.MILLISECOND:
        return Time.MILLISECOND_VALUES;
    }
  };
}
// See platform/date_components.h.
Time.Minimum = new Time(0, 0, 0, 0);
Time.Maximum = new Time(23, 59, 59, 999);
Time.ISOStringRegExp = /^(\d+):(\d+):?(\d*).?(\d*)/;
Time.HOUR_VALUES = 24;
Time.MINUTE_VALUES = 60;
Time.SECOND_VALUES = 60;
Time.MILLISECOND_VALUES = 10;

/**
 * TimePicker: Custom element providing a time picker implementation.
 *             TimePicker contains 2 parts:
 *                 - column container
 *                 - submission controls
 */
class TimePicker extends HTMLElement {
  constructor(config) {
    super();

    this.className = TimePicker.ClassName;
    this.initializeFromConfig_(config);

    this.timeColumns_ = new TimeColumns(this);
    this.submissionControls_ = new SubmissionControls(
        this.onSubmitButtonClick_, this.onCancelButtonClick_);
    this.append(this.timeColumns_, this.submissionControls_);
  }

  initializeFromConfig_ = (config) => {
    const initialSelection = parseDateTimeString(config.currentValue);
    this.selectedTime_ = initialSelection ? initialSelection
                                          : Time.currentTime();
    this.hasSecond_ = config.hasSecond;
    this.hasMillisecond_ = config.hasMillisecond;
  }

  onSubmitButtonClick_ = () => {
    const selectedValue = this.timeColumns_.selectedValue().toString(this.hasSecond, this.hasMillisecond);
    window.setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, selectedValue);
    }, 100);
  }

  onCancelButtonClick_ = () => {
    window.pagePopupController.closePopup();
  }

  get selectedTime() {
    return this.selectedTime_;
  }

  get hasSecond() {
    return this.hasSecond_;
  }

  get hasMillisecond() {
    return this.hasMillisecond_;
  }

  get width() {
    return this.timeColumns_.width;
  }

  get height() {
    return TimePicker.Height;
  }

  get timeColumns() {
    return this.timeColumns_;
  }

  get submissionControls() {
    return this.submissionControls_;
  }
}
TimePicker.ClassName = 'time-picker';
TimePicker.Height = 273;
TimePicker.ColumnWidth = 54;
window.customElements.define('time-picker', TimePicker);

/**
 * TimeColumns: Columns container that provides functionality for creating
 *              the required columns and for updating the selected value.
 */
class TimeColumns extends HTMLElement {
  constructor(timePicker) {
    super();

    this.className = TimeColumns.ClassName;
    this.hourColumn_ = new TimeColumn(TimeColumnType.HOUR, timePicker);
    this.width_ = TimePicker.ColumnWidth;
    this.minuteColumn_ = new TimeColumn(TimeColumnType.MINUTE, timePicker);
    this.append(this.hourColumn_, this.minuteColumn_);
    this.width_ += TimePicker.ColumnWidth;
    if (timePicker.hasSecond) {
      this.secondColumn_ = new TimeColumn(TimeColumnType.SECOND, timePicker);
      this.append(this.secondColumn_);
      this.width_ += TimePicker.ColumnWidth;
    }
    if (timePicker.hasMillisecond) {
      this.millisecondColumn_ =
          new TimeColumn(TimeColumnType.MILLISECOND, timePicker);
      this.append(this.millisecondColumn_);
      this.width_ += TimePicker.ColumnWidth;
    }
  }

  get width() {
    return this.width_;
  }

  selectedValue = () => {
    const hour = parseInt(this.hourColumn_.selectedTimeCell.value, 10);
    const minute = parseInt(this.minuteColumn_.selectedTimeCell.value, 10);
    const second = this.secondColumn_ ?
        parseInt(this.secondColumn_.selectedTimeCell.value, 10) :
        0;
    const millisecond = this.millisecondColumn_ ?
        parseInt(this.millisecondColumn_.selectedTimeCell.value, 10) :
        0;
    return new Time(hour, minute, second, millisecond);
  }
}
TimeColumns.ClassName = 'time-columns';
window.customElements.define('time-columns', TimeColumns);

/**
 * TimeColumn: Column that contains all values available for a time column type.
 */
class TimeColumn extends HTMLUListElement {
  constructor(columnType, timePicker) {
    super();

    this.className = TimeColumn.ClassName;
    this.columnType_ = columnType;
    this.createAndInitializeCells_(timePicker);

    this.addEventListener('click', this.onClick_);
  }

  createAndInitializeCells_ = (timePicker) => {
    const totalCells = Time.numberOfValues(this.columnType_);
    let currentTime = timePicker.selectedTime;
    let cells = [];
    for (let i = 0; i < totalCells; i++) {
      let timeCell = new TimeCell(currentTime.value(this.columnType_));
      cells.push(timeCell);
      currentTime.next(this.columnType_);
    }
    this.selectedTimeCell = cells[0];
    this.append(...cells);
  }

  onClick_ = (event) => {
    this.selectedTimeCell = event.target;
  }

  get selectedTimeCell() {
    return this.selectedTimeCell_;
  }

  set selectedTimeCell(timeCell) {
    if (this.selectedTimeCell_) {
      this.selectedTimeCell_.classList.remove('selected');
    }
    this.selectedTimeCell_ = timeCell;
    this.selectedTimeCell_.classList.add('selected');
  }
}
TimeColumn.ClassName = 'time-column';
window.customElements.define('time-column', TimeColumn, {extends: 'ul'});

/**
 * TimeCell: List item with a custom look that displays a time value.
 */
class TimeCell extends HTMLLIElement {
  constructor(value) {
    super();

    this.className = TimeCell.ClassName;
    this.textContent = localizeNumber(value);
    this.value = value;
  }
}
TimeCell.ClassName = 'time-cell';
window.customElements.define('time-cell', TimeCell, {extends: 'li'});

/**
 * SubmissionControls: Provides functionality to submit or discard a change.
 */
class SubmissionControls extends HTMLElement {
  constructor(submitCallback, cancelCallback) {
    super();

    const padding = document.createElement('span');
    padding.setAttribute('id', 'submission-controls-padding');
    this.append(padding);

    this.className = SubmissionControls.ClassName;

    this.submitButton_ = new SubmissionButton(
        submitCallback,
        '<svg width="14" height="10" viewBox="0 0 14 10" fill="none" ' +
            'xmlns="http://www.w3.org/2000/svg"><path d="M13.3516 ' +
            '1.35156L5 9.71094L0.648438 5.35156L1.35156 4.64844L5 ' +
            '8.28906L12.6484 0.648438L13.3516 1.35156Z" fill="black"/></svg>');
    this.cancelButton_ = new SubmissionButton(
        cancelCallback,
        '<svg width="14" height="14" viewBox="0 0 14 14" fill="none" ' +
            'xmlns="http://www.w3.org/2000/svg"><path d="M7.71094 7L13.1016 ' +
            '12.3984L12.3984 13.1016L7 7.71094L1.60156 13.1016L0.898438 ' +
            '12.3984L6.28906 7L0.898438 1.60156L1.60156 0.898438L7 ' +
            '6.28906L12.3984 0.898438L13.1016 1.60156L7.71094 7Z" ' +
            'fill="black"/></svg>');
    this.append(this.submitButton_, this.cancelButton_);
  }

  get submitButton() {
    return this.submitButton_;
  }

  get cancelButton() {
    return this.cancelButton_;
  }
}
SubmissionControls.ClassName = 'submission-controls';
window.customElements.define('submission-controls', SubmissionControls);

/**
 * SubmissionButton: Button with a custom look that can be clicked for
 *                   a submission action.
 */
class SubmissionButton extends HTMLButtonElement {
  constructor(clickCallback, htmlString) {
    super();

    this.className = SubmissionButton.ClassName;
    this.innerHTML = htmlString;
    this.addEventListener('click', clickCallback);
  }
}
SubmissionButton.ClassName = 'submission-button';
window.customElements.define(
    'submission-button', SubmissionButton, {extends: 'button'});
