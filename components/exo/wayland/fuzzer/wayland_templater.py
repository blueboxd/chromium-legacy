# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Templatize a file based on wayland specifications.

The templating engine takes an input template and one or more wayland
specifications (see third_party/wayland/src/protocol/wayland.dtd), and
instantiates the template based on the wayland content.
"""

from __future__ import absolute_import
from __future__ import print_function

import argparse
import sys
from xml.etree import ElementTree

import jinja2

proto_type_conversions = {
    'object': 'small_value',
    'int': 'int32',
    'uint': 'uint32',
    'string': 'string',
    'fd': 'uint32',
}

cpp_type_conversions = {
    'int': 'int32_t',
    'uint': 'uint32_t',
    'fixed': 'wl_fixed_t',
    'string': 'const char*',
    'array': 'struct wl_array*',
    'fd': 'int32_t',
}


def AllInterfaces(protocols):
  """Get the interfaces in these protocols.

  Args:
    protocols: the list of protocols you want the interfaces of.

  Yields:
    Tuples (p, i) of (p)rotocol (i)nterface.
  """
  for p in protocols:
    for i in p.findall('interface'):
      yield (p, i)


def AllMessages(protocols):
  """Get the messages in these protocols.

  Args:
    protocols: the list of protocols you want the messages of.

  Yields:
    Tuples (p, i, m) of (p)rotocol, (i)nterface, and (m)essage.
  """
  for (p, i) in AllInterfaces(protocols):
    for r in i.findall('request'):
      yield (p, i, r)
    for e in i.findall('event'):
      yield (p, i, e)


def GetConstructorArg(message):
  """Get the argument that this message constructs, or None.

  Args:
    message: the message which you want to find the constructor arg of.

  Returns:
    The argument (as an ElementTree node) that constructs a new interface, or
    None.
  """
  return message.find('arg[@type="new_id"]')


def IsConstructor(message):
  """Check if a message is a constructor.

  Args:
    message: the message which you want to check.

  Returns:
    True if the message constructs an object (via new_id), False otherwise.
  """
  return GetConstructorArg(message) is not None


def GetConstructedInterface(message):
  """Gets the interface constructed by a message.

  Note that even if IsConstructor(message) returns true, get_constructed can
  still return None when the message constructs an unknown interface (e.g.
  wl_registry.bind()).

  Args:
    message: the event/request which may be a constructor.

  Returns:
    The name of the constructed interface (if there is one), or None.
  """
  cons_arg = GetConstructorArg(message)
  if cons_arg is None:
    return None
  return cons_arg.attrib.get('interface')


def NeedsListener(interface):
  return interface.find('event') is not None


def GetCppType(arg):
  ty = arg.attrib['type']
  if ty in ['object', 'new_id']:
    return ('::' if 'interface' in arg.attrib else '') + arg.attrib.get(
        'interface', 'void') + '*'
  return cpp_type_conversions[ty]


class TemplaterContext(object):
  """The context object used for recording stateful/expensive things.

  An instance of this class is used when generating the template data, we use
  it to cache pre-computed information, as well as to side-effect stateful
  queries (such as counters) while generating the template data.
  """

  def __init__(self, protocols):
    self.non_global_names = {
        GetConstructedInterface(m) for _, _, m in AllMessages(protocols)
    } - {None}
    self.interfaces_with_listeners = {
        i.attrib['name']
        for p, i in AllInterfaces(protocols)
        if NeedsListener(i)
    }
    self.counts = {}

  def GetAndIncrementCount(self, counter):
    """Return the number of times the given key has been counted.

    Args:
      counter: the key used to identify this count value.

    Returns:
      An int which is the number of times this method has been called before
      with this counter's key.
    """
    self.counts[counter] = self.counts.get(counter, 0) + 1
    return self.counts[counter] - 1


def GetArg(arg):
  ty = arg.attrib['type']
  return {
      'name': arg.attrib['name'],
      'type': ty,
      'proto_type': proto_type_conversions.get(ty),
      'cpp_type': GetCppType(arg),
      'interface': arg.attrib.get('interface'),
  }


def GetMessage(message, context):
  name = message.attrib['name']
  constructed = GetConstructedInterface(message)
  return {
      'name':
          name,
      'idx':
          context.GetAndIncrementCount('message_index'),
      'args': [GetArg(a) for a in message.findall('arg')],
      'is_constructor':
          IsConstructor(message),
      'constructed':
          constructed,
      'constructed_has_listener':
          constructed in context.interfaces_with_listeners,
  }


def GetInterface(interface, context):
  name = interface.attrib['name']
  return {
      'name':
          name,
      'idx':
          context.GetAndIncrementCount('interface_index'),
      'is_global':
          name not in context.non_global_names,
      'events': [GetMessage(m, context) for m in interface.findall('event')],
      'requests': [
          GetMessage(m, context) for m in interface.findall('request')
      ],
      'has_listener':
          NeedsListener(interface)
  }


def GetTemplateData(protocol_paths):
  protocols = [ElementTree.parse(path).getroot() for path in protocol_paths]
  context = TemplaterContext(protocols)
  interfaces = []
  for p in protocols:
    for i in p.findall('interface'):
      interfaces.append(GetInterface(i, context))
  return {
      'protocol_names': [p.attrib['name'] for p in protocols],
      'interfaces': interfaces,
  }


def main(argv):
  """Execute the templater, based on the user provided args.

  Args:
    argv: the command line arguments (including the script name)
  """
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument(
      '-d',
      '--directory',
      help='treat input paths as relative to this directory',
      default='.')
  parser.add_argument(
      '-i',
      '--input',
      help='path to the input template file (relative to -d)',
      required=True)
  parser.add_argument(
      '-o',
      '--output',
      help='path to write the generated file to',
      required=True)
  parser.add_argument(
      '-s',
      '--spec',
      help='path(s) to the wayland specification(s)',
      nargs='+',
      required=True)
  parsed_args = parser.parse_args(argv[1:])

  env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(parsed_args.directory),
      keep_trailing_newline=True,  # newline-terminate generated files
      lstrip_blocks=True,
      trim_blocks=True)  # so don't need {%- -%} everywhere
  template = env.get_template(parsed_args.input)
  with open(parsed_args.output, 'w') as out_fi:
    out_fi.write(template.render(GetTemplateData(parsed_args.spec)))


if __name__ == '__main__':
  main(sys.argv)
