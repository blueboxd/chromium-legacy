// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import Vue from 'vue';
import ClassGraphPage from './vue_components/class_graph_page.vue';
import {loadGraph} from './load_graph.js';
import * as d3 from 'd3';

document.addEventListener('DOMContentLoaded', () => {
  loadGraph().then(data => {
    new Vue({
      el: '#class-graph-page',
      render: createElement => createElement(
          ClassGraphPage,
          {
            props: {
              graphJson: data.class_graph,
            },
          },
      ),
    });
  }).catch(e => {
    document.write("Error loading graph.");
  });
});
