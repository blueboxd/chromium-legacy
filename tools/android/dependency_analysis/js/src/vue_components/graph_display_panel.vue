<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div id="display-panel">
    <div id="preset-container">
      <select
          v-model="internalDisplaySettingsPreset"
          @change="applySelectedPreset">
        <option
            v-for="presetName in DisplaySettingsPreset"
            :key="presetName"
            :value="presetName">
          {{ presetName }}
        </option>
      </select>
      <button @click="settingsExpanded = !settingsExpanded">
        {{ settingsExpanded ? 'Collapse' : 'Expand' }} Advanced Settings
      </button>
    </div>
    <slot v-if="settingsExpanded"/>
  </div>
</template>

<script>
import {DisplaySettingsPreset} from '../display_settings_data.js';

// @vue/component
const GraphDisplaySettings = {
  props: {
    displaySettingsData: Object,
    displaySettingsPreset: String,
  },
  data: function() {
    return {
      settingsExpanded: false,
    };
  },
  computed: {
    DisplaySettingsPreset: () => DisplaySettingsPreset,
    internalDisplaySettingsPreset: {
      get: function() {
        return this.displaySettingsPreset;
      },
      set: function(newValue) {
        this.$emit('update:displaySettingsPreset', newValue);
      },
    },
  },
  methods: {
    applySelectedPreset() {
      // nextTick is needed here since we need to wait for the parent/child data
      // sync on `displaySettingsPreset` to finish.
      this.$nextTick(() => this.displaySettingsData.applyPreset(
          this.internalDisplaySettingsPreset));
    },
  },
};

export default GraphDisplaySettings;
</script>

<style scoped>
#preset-container {
  display: flex;
  flex-direction: row;
}

#display-panel {
  display: flex;
  flex-direction: column;
  margin-bottom: 10px;
}
</style>
