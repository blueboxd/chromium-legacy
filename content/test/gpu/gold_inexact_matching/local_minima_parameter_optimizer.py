# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import itertools
import logging
import sys

import iterative_parameter_optimizer as iterative_optimizer
import parameter_set


class LocalMinimaParameterOptimizer(
    iterative_optimizer.IterativeParameterOptimizer):
  """A ParameterOptimizer to find local minima.

  Works on any number of variable parameters and is faster than brute
  forcing, but not guaranteed to find all interesting parameter combinations.
  """
  MIN_EDGE_THRESHOLD_WEIGHT = 0
  MIN_MAX_DIFF_WEIGHT = MIN_DELTA_THRESHOLD_WEIGHT = 0

  @classmethod
  def AddArguments(cls, parser):
    common_group, sobel_group, fuzzy_group = super(
        LocalMinimaParameterOptimizer, cls).AddArguments(parser)

    common_group.add_argument(
        '--use-bfs',
        action='store_true',
        default=False,
        help='Use a breadth-first search instead of a depth-first search. This '
        'will likely be significantly slower, but is more likely to find '
        'multiple local minima with the same weight.')

    sobel_group.add_argument(
        '--edge-threshold-weight',
        default=1,
        type=int,
        help='The weight associated with the edge threshold. Higher values '
        'will penalize a more permissive parameter value more harshly.')

    fuzzy_group.add_argument(
        '--max-diff-weight',
        default=3,
        type=int,
        help='The weight associated with the maximum number of different '
        'pixels. Higher values will penalize a more permissive parameter value '
        'more harshly.')
    fuzzy_group.add_argument(
        '--delta-threshold-weight',
        default=10,
        type=int,
        help='The weight associated with the per-channel delta sum. Higher '
        'values will penalize a more permissive parameter value more harshly.')

    return common_group, sobel_group, fuzzy_group

  def _VerifyArgs(self):
    super(LocalMinimaParameterOptimizer, self)._VerifyArgs()

    assert self._args.edge_threshold_weight >= self.MIN_EDGE_THRESHOLD_WEIGHT

    assert self._args.max_diff_weight >= self.MIN_MAX_DIFF_WEIGHT
    assert self._args.delta_threshold_weight >= self.MIN_DELTA_THRESHOLD_WEIGHT

  def _RunOptimizationImpl(self):
    visited_parameters = set()
    to_visit = collections.deque()
    smallest_weight = sys.maxsize
    smallest_parameters = []

    to_visit.append(self._GetMostPermissiveParameters())
    # Do a search, only considering adjacent parameters if:
    # 1. Their weight is less than or equal to the smallest found weight.
    # 2. They haven't been visited already.
    # 3. The current parameters result in a successful comparison.
    while len(to_visit):
      current_parameters = None
      if self._args.use_bfs:
        current_parameters = to_visit.popleft()
      else:
        current_parameters = to_visit.pop()
      weight = self._GetWeight(current_parameters)
      if weight > smallest_weight:
        continue
      if current_parameters in visited_parameters:
        continue
      visited_parameters.add(current_parameters)
      success, _, _ = self._RunComparisonForParameters(current_parameters)
      if success:
        for adjacent in self._AdjacentParameters(current_parameters):
          to_visit.append(adjacent)
        if smallest_weight == weight:
          logging.info('Found additional smallest parameter %s',
                       current_parameters)
          smallest_parameters.append(current_parameters)
        else:
          logging.info('Found new smallest parameter with weight %d: %s',
                       weight, current_parameters)
          smallest_weight = weight
          smallest_parameters = [current_parameters]
    print 'Found %d parameter(s) with the smallest weight:' % len(
        smallest_parameters)
    for p in smallest_parameters:
      print p

  def _AdjacentParameters(self, starting_parameters):
    max_diff = starting_parameters.max_diff
    delta_threshold = starting_parameters.delta_threshold
    edge_threshold = starting_parameters.edge_threshold

    max_diff_step = self._args.max_diff_step
    delta_threshold_step = self._args.delta_threshold_step
    edge_threshold_step = self._args.edge_threshold_step

    max_diffs = [
        max(self._args.min_max_diff, max_diff - max_diff_step), max_diff,
        min(self._args.max_max_diff, max_diff + max_diff_step)
    ]
    delta_thresholds = [
        max(self._args.min_delta_threshold,
            delta_threshold - delta_threshold_step), delta_threshold,
        min(self._args.max_delta_threshold,
            delta_threshold + delta_threshold_step)
    ]
    edge_thresholds = [
        max(self._args.min_edge_threshold,
            edge_threshold - edge_threshold_step), edge_threshold,
        min(self._args.max_edge_threshold, edge_threshold + edge_threshold_step)
    ]
    for combo in itertools.product(max_diffs, delta_thresholds,
                                   edge_thresholds):
      adjacent = parameter_set.ParameterSet(combo[0], combo[1], combo[2])
      if adjacent != starting_parameters:
        yield adjacent

  def _GetWeight(self, parameters):
    return (parameters.max_diff * self._args.max_diff_weight +
            parameters.delta_threshold * self._args.delta_threshold_weight +
            (self.MAX_EDGE_THRESHOLD - parameters.edge_threshold) *
            self._args.edge_threshold_weight)
