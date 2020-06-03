# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Various helpers for printing dependencies."""

from typing import List


def get_valid_keys_matching_input(all_keys: List, input_key: str) -> List[str]:
    """Return a list of valid keys into the graph's nodes based on an input.

    For our use case (matching user input to graph nodes),
    a valid key is one that ends with the input, case insensitive.
    For example, 'apphooks' matches 'org.chromium.browser.AppHooks'.
    """
    input_key_lower = input_key.lower()
    return [key for key in all_keys if key.lower().endswith(input_key_lower)]
