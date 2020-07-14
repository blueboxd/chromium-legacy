// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CUSTOM_EVENTS} from '../vue_custom_events.js';

const GraphFilterItems = {
  props: ['nodeFilterData'],
  data: function() {
    return this.nodeFilterData;
  },
  methods: {
    removeFilter: function(e) {
      const filterText = e.target.textContent;
      this.$emit(CUSTOM_EVENTS.FILTER_ELEMENT_CLICKED, filterText.trim());
    },
  },
  template: `
    <ul class="filter-items">
      <li @click="removeFilter" v-for="node in nodeList">
        {{node}}
      </li>
    </ul>`,
};

export {
  GraphFilterItems,
};
