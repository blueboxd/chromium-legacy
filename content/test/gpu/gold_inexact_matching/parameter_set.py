# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class ParameterSet(object):
  """Struct-like object for holding parameters for an iteration."""

  def __init__(self, max_diff, delta_threshold, edge_threshold):
    """
    Args:
      max_diff: The maximum number of pixels that are allowed to differ.
      delta_threshold: The maximum per-channel delta sum that is allowed.
      edge_threshold: The threshold for what is considered an edge for a
          Sobel filter.
    """
    self.max_diff = max_diff
    self.delta_threshold = delta_threshold
    self.edge_threshold = edge_threshold

  def AsList(self):
    """Returns the object's data in list format.

    The returned object is suitable for appending to a "goldctl match" command
    in order to compare using the parameters stored within the object.

    Returns:
      A list of strings.
    """
    return [
        '--parameter',
        'fuzzy_max_different_pixels:%d' % self.max_diff,
        '--parameter',
        'fuzzy_pixel_delta_threshold:%d' % self.delta_threshold,
        '--parameter',
        'sobel_edge_threshold:%d' % self.edge_threshold,
    ]

  def __str__(self):
    return ('Max different pixels: %d, Max per-channel delta sum: %d, Sobel '
            'edge threshold: %d') % (self.max_diff, self.delta_threshold,
                                     self.edge_threshold)

  def __eq__(self, other):
    return (self.max_diff == other.max_diff
            and self.delta_threshold == other.delta_threshold
            and self.edge_threshold == other.edge_threshold)

  def __hash__(self):
    return hash((self.max_diff, self.delta_threshold, self.edge_threshold))
