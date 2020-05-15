# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class SimpleCanvasPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.SIMPLE_CANVAS]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(SimpleCanvasPage,
          self).__init__(page_set=page_set,
                         shared_page_state_class=shared_page_state_class,
                         name_suffix=name_suffix,
                         extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(SimpleCanvasPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CanvasAnimation'):
      action_runner.Wait(10)


class CanvasToCanvasDrawPage(SimpleCanvasPage):
  BASE_NAME = 'canvas_to_canvas_draw'
  URL = 'file://../simple_canvas/canvas_to_canvas_draw.html'
