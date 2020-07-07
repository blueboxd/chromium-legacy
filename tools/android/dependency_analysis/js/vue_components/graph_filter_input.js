// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphFilterInput = {
  data: function() {
    return {
      filterInputText: '',
    };
  },
  methods: {
    submitFilter() {
      this.$emit(CUSTOM_EVENTS.FILTER_SUBMITTED, this.filterInputText);
    },
  },
  template: `
    <div class="user-input-group">
      <label for="filter-input">Add node to filter (exact name):</label>
      <input v-model="filterInputText" type="text" id="filter-input">
      <button @click="submitFilter" type="button">Add</button>
    </div>`,
};

export {
  GraphFilterInput,
};
