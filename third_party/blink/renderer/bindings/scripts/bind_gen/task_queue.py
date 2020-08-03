# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing

from .package_initializer import package_initializer


class TaskQueue(object):
    """
    Represents a task queue to run tasks with using a worker pool.  Scheduled
    tasks will be executed in parallel.
    """

    def __init__(self):
        self._pool_size = multiprocessing.cpu_count()
        self._pool = multiprocessing.Pool(self._pool_size,
                                          package_initializer().init)
        self._requested_tasks = []  # List of (func, args, kwargs)
        self._worker_tasks = []  # List of multiprocessing.pool.AsyncResult
        self._did_run = False

    def post_task(self, func, *args, **kwargs):
        """
        Schedules a new task to be executed when |run| method is invoked.  This
        method does not kick any execution, only puts a new task in the queue.
        """
        assert not self._did_run
        self._requested_tasks.append((func, args, kwargs))

    def run(self, report_progress=None):
        """
        Executes all scheduled tasks.

        Args:
            report_progress: A callable that takes two arguments, total number
                of worker tasks and number of completed worker tasks.
                Scheduled tasks are reorganized into worker tasks, so the
                number of worker tasks may be different from the number of
                scheduled tasks.
        """
        assert report_progress is None or callable(report_progress)
        assert not self._did_run
        assert not self._worker_tasks
        self._did_run = True

        num_of_requested_tasks = len(self._requested_tasks)
        chunk_size = 1
        i = 0
        while i < num_of_requested_tasks:
            tasks = self._requested_tasks[i:i + chunk_size]
            i += chunk_size
            self._worker_tasks.append(
                self._pool.apply_async(_task_queue_run_tasks, [tasks]))
        self._pool.close()

        timeout_in_sec = 1
        while True:
            self._report_worker_task_progress(report_progress)
            for worker_task in self._worker_tasks:
                if not worker_task.ready():
                    worker_task.wait(timeout_in_sec)
                    break
                if not worker_task.successful():
                    worker_task.get()  # Let |get()| raise an exception.
                    assert False
            else:
                break

        self._pool.join()

    def _report_worker_task_progress(self, report_progress):
        assert report_progress is None or callable(report_progress)

        if not report_progress:
            return

        done_count = reduce(
            lambda count, worker_task: count + bool(worker_task.ready()),
            self._worker_tasks, 0)
        report_progress(len(self._worker_tasks), done_count)


def _task_queue_run_tasks(tasks):
    for task in tasks:
        func, args, kwargs = task
        apply(func, args, kwargs)
