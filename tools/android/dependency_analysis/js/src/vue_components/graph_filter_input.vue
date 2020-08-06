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
        :get-result-value="getResultValue"
        @submit="onSelectOption"/>
  </div>
</template>

<script>
import {CUSTOM_EVENTS} from '../vue_custom_events.js';

import Autocomplete from '@trevoreyre/autocomplete-vue';

// @vue/component
const GraphFilterInput = {
  components: {
    Autocomplete,
  },
  props: {
    nodeIds: Array,
    shortenName: Function,
  },
  data: function() {
    return {
      // Sorts the nodes by their shortened names, which will be displayed.
      // this.shortenName() is cached to improve performance (~150 ms at load).
      nodeIdsSortedByShortNames: this.nodeIds
          .map(name => ({
            realName: name,
            shortName: this.shortenName(name),
          }))
          .sort((a, b) => a.shortName.localeCompare(b.shortName))
          .map(nameObj => nameObj.realName),
    };
  },
  methods: {
    getResultValue: function(result) {
      return this.shortenName(result);
    },

    search: function(searchTerm) {
      return this.nodeIdsSortedByShortNames.filter(name => {
        return name.toLowerCase().includes(searchTerm.toLowerCase());
      });
    },

    onSelectOption(nodeNameToAdd) {
      if (!this.nodeIdsSortedByShortNames.includes(nodeNameToAdd)) {
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
