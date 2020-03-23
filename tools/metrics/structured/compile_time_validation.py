# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Verifies that structured.xml is well-structured."""

from model import _EVENT_TYPE, _EVENTS_TYPE
from model import _PROJECT_TYPE, _PROJECTS_TYPE


def eventsReferenceValidProjects(data):
  """Check that any project referenced by an event exists."""
  projects = {
      project['name']: project
      for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag]
  }
  for event in data[_EVENTS_TYPE.tag][_EVENT_TYPE.tag]:
    project_name = event.get('project')
    project = projects.get(project_name)
    if project is None and project_name is not None:
      raise Exception(("Structured metrics event '{}' references "
                       "nonexistent project '{}'.").format(
                           event['name'], project_name))


def projectAndEventNamesDontCollide(data):
  """Check that there are no events with the same name as a project."""
  projects = {
      project['name']
      for project in data[_PROJECTS_TYPE.tag][_PROJECT_TYPE.tag]
  }
  for event in data[_EVENTS_TYPE.tag][_EVENT_TYPE.tag]:
    if event['name'] in projects:
      raise Exception(("Structured metrics event and project have the same "
                       "name: '{}'.").format(event['name']))


def validate(data):
  eventsReferenceValidProjects(data)
  projectAndEventNamesDontCollide(data)
