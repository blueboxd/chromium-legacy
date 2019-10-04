# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.page import page as page_module


class PressStory(page_module.Page):
  """Base class for Press stories.

    Override ExecuteTest to execute javascript on the page and
    ParseTestResults to obtain javascript metrics from page.

    Example Implementation:

    class FooPressStory:
      URL = 'http://foo'

      def ExecuteTest(self, action_runner):
        //Execute some javascript

      def ParseTestResults(self, action_runner):
        some_value = action_runner.EvaluateJavascript("some javascript")
        self.AddJavascriptMetricValue(some_value)
  """
  URL = None
  DETERMINISTIC_JS = False
  NAME = None

  def __init__(self, ps):
    super(PressStory, self).__init__(
        self.URL, ps,
        base_dir=ps.base_dir,
        make_javascript_deterministic=self.DETERMINISTIC_JS,
        name=self.NAME if self.NAME else self.URL)
    self._values = []
    self._histogram_values = []

  def GetJavascriptMetricValues(self):
    return self._values

  def AddJavascriptMetricValue(self, value):
    self._values.append(value)

  def GetJavascriptMetricHistograms(self):
    return self._histogram_values

  def AddJavascriptMetricHistogram(self, value):
    self._histogram_values.append(value)

  def ExecuteTest(self, action_runner):
    pass

  def ParseTestResults(self, action_runner):
    pass

  def RunPageInteractions(self, action_runner):
    self.ExecuteTest(action_runner)
    self.ParseTestResults(action_runner)
