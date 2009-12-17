# Copyright (C) 2009 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
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

import unittest

from modules.buildsteps import UpdateChangelogsWithReviewerStep, UpdateStep
from modules.mock_bugzillatool import MockBugzillaTool
from modules.outputcapture import OutputCapture
from modules.mock import Mock


class UpdateChangelogsWithReviewerStepTest(unittest.TestCase):
    def test_guess_reviewer_from_bug(self):
        capture = OutputCapture()
        step = UpdateChangelogsWithReviewerStep(MockBugzillaTool(), [])
        expected_stderr = "0 reviewed patches on bug 1, cannot infer reviewer.\n"
        capture.assert_outputs(self, step._guess_reviewer_from_bug, [1], expected_stderr=expected_stderr)


class StepsTest(unittest.TestCase):
    def _run_step(self, step, options, state=None):
        if not state:
            state = {}
        step(MockBugzillaTool(), options).run(state)

    def test_update_step(self):
        options = Mock()
        options.update = True
        self._run_step(UpdateStep, options)
