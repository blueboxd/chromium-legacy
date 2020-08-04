<!-- Copyright 2020 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->

<template>
  <div class="numeric-input-container">
    <label :for="inputId">{{ description }}</label>
    <div class="input-and-button-container">
      <input
          :id="inputId"
          v-model="internalInputValue"
          type="number">
      <div class="button-group">
        <button @click="internalInputValue++">
          +
        </button>
        <button @click="internalInputValue--">
          -
        </button>
      </div>
    </div>
  </div>
</template>

<script>
// @vue/component
const NumericInput = {
  props: {
    description: String,
    inputId: String,
    inputValue: Number,
  },
  computed: {
    internalInputValue: {
      get: function() {
        return this.inputValue;
      },
      set: function(newValue) {
        this.$emit('update:inputValue', newValue);
      },
    },
  },
};

export default NumericInput;
</script>

<style scoped>
.numeric-input-container {
  display: flex;
  flex-direction: column;
  max-width: 150px;
}

.input-and-button-container {
  display: flex;
  flex-direction: row;
}

.button-group {
  display: flex;
  flex-direction: column;
}

input {
  width: 100%;
}

input[type=number]::-webkit-inner-spin-button,
input[type=number]::-webkit-outer-spin-button {
  -webkit-appearance: none;
  margin: 0;
}
</style>
