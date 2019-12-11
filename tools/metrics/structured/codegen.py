# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Objects for describing template code to be generated from structured.xml."""

import hashlib
import os
import re
import struct
from model import _EVENT_TYPE, _METRIC_TYPE

def sanitize_name(name):
  s = re.sub('[^0-9a-zA-Z_]', '_', name)
  return s


def HashName(name):
  # This must match the hash function in base/metrics/metric_hashes.cc
  # >Q: 8 bytes, big endian.
  return struct.unpack('>Q', hashlib.md5(name).digest()[:8])[0]


class FileInfo(object):
  def __init__(self, relpath, basename):
    self.dir_path = relpath
    self.guard_path = sanitize_name(os.path.join(relpath, basename)).upper()


class EventInfo(object):
  def __init__(self, json_obj):
    self.raw_name = json_obj['name']
    self.name = sanitize_name(json_obj['name'])
    self.hash = HashName(json_obj['name'])


class MetricInfo(object):
  def __init__(self, json_obj):
    self.raw_name = json_obj['name']
    self.name = sanitize_name(json_obj['name'])
    self.hash = HashName(json_obj['name'])
    if json_obj['kind'] == 'hashed-string':
      self.type = 'std::string&'
      self.setter = 'AddStringMetric'
    elif json_obj['kind'] == 'int':
      self.type = 'int'
      self.setter = 'AddIntMetric'
    else:
      raise Exception("Unexpected metric kind: " + json_obj['kind'])

class Template(object):
  """Template for producing code from structured.xml."""

  def __init__(self, basename, file_template, event_template, metric_template):
    self.basename = basename
    self.file_template = file_template
    self.event_template = event_template
    self.metric_template = metric_template

  def _StampMetricCode(self, file_info, event_info, metric):
    return self.metric_template.format(
        file=file_info,
        event=event_info,
        metric=MetricInfo(metric))

  def _StampEventCode(self, file_info, event):
    event_info = EventInfo(event)
    metric_code = ''.join(
        self._StampMetricCode(file_info, event_info, metric)
        for metric in event[_METRIC_TYPE.tag])
    return self.event_template.format(
        file=file_info,
        event=event_info,
        metric_code=metric_code)

  def _StampFileCode(self, relpath, data):
    file_info = FileInfo(relpath, self.basename)
    event_code = "".join(
        self._StampEventCode(file_info, event)
        for event in data[_EVENT_TYPE.tag])
    event_name_hashes = ['UINT64_C(%s)' % HashName(metric['name'])
                         for metric in data[_EVENT_TYPE.tag]]
    event_name_hashes = '{' + ', '.join(event_name_hashes) + '}'
    return self.file_template.format(
        file=file_info,
        event_code=event_code,
        event_name_hashes=event_name_hashes)

  def WriteFile(self, outdir, relpath, data):
    """Generates code and writes it to a file.

    Args:
      relpath: The path to the file in the source tree.
      rootdir: The root of the path the file should be written to.
      data: The parsed structured.xml data.
    """
    output = open(os.path.join(outdir, self.basename), 'w')
    output.write(self._StampFileCode(relpath, data))
    output.close()
