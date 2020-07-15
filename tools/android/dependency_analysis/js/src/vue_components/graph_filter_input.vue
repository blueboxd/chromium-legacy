<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="user-input-group">
    <label for="filter-input">Add node to filter (exact name):</label>
    <Autocomplete
        id="filter-input"
        ref="autocomplete"
        :search="search"
        @submit="onSelectOption"/>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

import Autocomplete from '@trevoreyre/autocomplete-vue';

const GraphFilterInput = {
  components: {
    Autocomplete,
  },
  props: {
    'nodeIds': Array,
  },
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
};

export default GraphFilterInput;
</script>

<style>
#filter-input {
  width: 500px;
}

.autocomplete-result-list {
  background: #fff;
  box-sizing: content-box;
  list-style: none;
  max-height: 600px;
  overflow-y: auto;
  padding: 0;
}

.autocomplete-result:hover,
.autocomplete-result[aria-selected=true] {
  background-color: rgba(0, 0, 0, 0.1);
}
</style>
