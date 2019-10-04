# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import multiprocessing
from multiprocessing.dummy import Pool as ThreadPool


def ApplyInParallel(function, work_list):
  """Apply a function to all values in work_list in parallel.

  Args:
    function: A function with one argument.
    work_list: Any iterable with arguments for the function.

  Returns:
    A generator over results. The order of results might not match the
    order of the arguments in the work_list.
  """
  try:
    # Note that this is speculatively halved as an attempt to fix
    # crbug.com/953365.
    cpu_count = multiprocessing.cpu_count() / 2
  except NotImplementedError:
    # Some platforms can raise a NotImplementedError from cpu_count()
    logging.warning('cpu_count() not implemented.')
    cpu_count = 4
  pool = ThreadPool(min(cpu_count, len(work_list)))

  try:
    for result in pool.imap_unordered(function, work_list):
      yield result
    pool.close()
    pool.join()
  finally:
    pool.terminate()
