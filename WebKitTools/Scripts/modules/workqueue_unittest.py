#!/usr/bin/env python
# Copyright (c) 2009, Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
# 
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import shutil
import tempfile
import unittest

from modules.executive import ScriptError
from modules.workqueue import WorkQueue, WorkQueueDelegate

class LoggingDelegate(WorkQueueDelegate):
    def __init__(self, test):
        self._test = test
        self._callbacks = []
        self._run_before = False

    expected_callbacks = [
        'queue_log_path',
        'begin_work_queue',
        'should_continue_work_queue',
        'next_work_item',
        'should_proceed_with_work_item',
        'work_item_log_path',
        'process_work_item',
        'should_continue_work_queue'
    ]

    def record(self, method_name):
        self._callbacks.append(method_name)

    def queue_log_path(self):
        self.record("queue_log_path")
        return os.path.join(self._test.temp_dir, "queue_log_path")

    def work_item_log_path(self, work_item):
        self.record("work_item_log_path")
        return os.path.join(self._test.temp_dir, "work_log_path", "%s.log" % work_item)

    def begin_work_queue(self):
        self.record("begin_work_queue")

    def should_continue_work_queue(self):
        self.record("should_continue_work_queue")
        if not self._run_before:
            self._run_before = True
            return True
        return False

    def next_work_item(self):
        self.record("next_work_item")
        return "work_item"

    def should_proceed_with_work_item(self, work_item):
        self.record("should_proceed_with_work_item")
        self._test.assertEquals(work_item, "work_item")
        fake_patch = { 'bug_id' : 42 }
        return (True, "waiting_message", fake_patch)

    def process_work_item(self, work_item):
        self.record("process_work_item")
        self._test.assertEquals(work_item, "work_item")

    def handle_unexpected_error(self, work_item, message):
        self.record("handle_unexpected_error")
        self._test.assertEquals(work_item, "work_item")


class ThrowErrorDelegate(LoggingDelegate):
    def __init__(self, test, error_code):
        LoggingDelegate.__init__(self, test)
        self.error_code = error_code

    def process_work_item(self, work_item):
        self.record("process_work_item")
        raise ScriptError(exit_code=self.error_code)


class NotSafeToProceedDelegate(LoggingDelegate):
    def should_proceed_with_work_item(self, work_item):
        self.record("should_proceed_with_work_item")
        self._test.assertEquals(work_item, "work_item")
        return False


class FastWorkQueue(WorkQueue):
    def __init__(self, delegate):
        WorkQueue.__init__(self, "fast-queue", delegate)

    # No sleep for the wicked.
    seconds_to_sleep = 0

    def _sleep(self, message):
        pass


class WorkQueueTest(unittest.TestCase):
    def test_trivial(self):
        delegate = LoggingDelegate(self)
        work_queue = WorkQueue("trivial-queue", delegate)
        work_queue.run()
        self.assertEquals(delegate._callbacks, LoggingDelegate.expected_callbacks)
        self.assertTrue(os.path.exists(os.path.join(self.temp_dir, "queue_log_path")))
        self.assertTrue(os.path.exists(os.path.join(self.temp_dir, "work_log_path", "work_item.log")))

    def test_unexpected_error(self):
        delegate = ThrowErrorDelegate(self, 3)
        work_queue = WorkQueue("error-queue", delegate)
        work_queue.run()
        expected_callbacks = LoggingDelegate.expected_callbacks[:]
        work_item_index = expected_callbacks.index('process_work_item')
        # The unexpected error should be handled right after process_work_item starts
        # but before any other callback.  Otherwise callbacks should be normal.
        expected_callbacks.insert(work_item_index + 1, 'handle_unexpected_error')
        self.assertEquals(delegate._callbacks, expected_callbacks)

    def test_handled_error(self):
        delegate = ThrowErrorDelegate(self, WorkQueue.handled_error_code)
        work_queue = WorkQueue("handled-error-queue", delegate)
        work_queue.run()
        self.assertEquals(delegate._callbacks, LoggingDelegate.expected_callbacks)

    def test_not_safe_to_proceed(self):
        delegate = NotSafeToProceedDelegate(self)
        work_queue = FastWorkQueue(delegate)
        work_queue.run()
        expected_callbacks = LoggingDelegate.expected_callbacks[:]
        next_work_item_index = expected_callbacks.index('next_work_item')
        # We slice out the common part of the expected callbacks.
        # We add 2 here to include should_proceed_with_work_item, which is
        # a pain to search for directly because it occurs twice.
        expected_callbacks = expected_callbacks[:next_work_item_index + 2]
        expected_callbacks.append('should_continue_work_queue')
        self.assertEquals(delegate._callbacks, expected_callbacks)

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp(suffix="work_queue_test_logs")

    def tearDown(self):
        shutil.rmtree(self.temp_dir)


if __name__ == '__main__':
    unittest.main()
