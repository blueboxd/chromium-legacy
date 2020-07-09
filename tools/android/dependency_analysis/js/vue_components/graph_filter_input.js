// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphFilterInput = {
  props: {'nodeIds': Array},
  methods: {
    search: function(searchTerm) {
      return this.nodeIds.filter(name => {
        return name.toLowerCase().includes(searchTerm.toLowerCase());
      });
    },

    onSelectOption(nodeNameToAdd) {
      if (!this.nodeIds.includes(nodeNameToAdd)) {
        return;
      }
      this.$emit(CUSTOM_EVENTS.FILTER_SUBMITTED, nodeNameToAdd);
      this.$refs.autocomplete.value = '';
    },
  },
  template: `
    <div class="user-input-group">
      <label for="filter-input">Add node to filter (exact name):</label>
      <autocomplete
          id="filter-input"
          ref="autocomplete"
          :search="search"
          @submit="onSelectOption"
      ></autocomplete>
    </div>`,
};

export {
  GraphFilterInput,
};
