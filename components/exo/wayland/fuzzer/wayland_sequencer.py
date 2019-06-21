# Copyright (c) 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generate sequences of valid wayland instructions.

Determines sequences of calls necessary to instantiate and invoke a given
message of one of the wayland interfaces. The procedure is as follows:
  - Get a dependency graph between messages, and the interfaces you need in
    order to invoke them.
  - The list of messages you need to invoke is the sum of transitive
    dependencies for the target message.
"""

from __future__ import absolute_import
from __future__ import print_function

import os
import sys

import wayland_templater
import wayland_utils as wlu

# To prevent the fuzzer from exploring too-much space, we limit it to binding at
# most a small number of each interface. So the index has to be in this map.
small_value = {0: 'ZERO', 1: 'ONE', 2: 'TWO', 3: 'THREE'}

# The default (empty) message is a wl_display_roundtrip.
round_trip = ('', '', [])


def ToNode(interface, message):
  return (interface.attrib['name'], message.attrib['name'])


def GetDependencyGraph(protocols):
  """Determine the dependencies between interfaces and their messages.

  Args:
    protocols: the list of wayland xml protocols you want the dependencies of.

  Returns:
    A bipartite graph where messages (i, m) depend on interfaces (i) and
    vice-versa. An edge from (i,m) to (i') indicates an i' instance is needed to
    invoke m, whereas (i) to (i',m') indicates m' is a constructor for i
    instances.
  """
  dep_graph = {}
  constructed_interfaces = set()
  for _, i, m in wlu.AllMessages(protocols):
    dep_graph[ToNode(i, m)] = [('receiver', i.attrib['name'])] + [
        (a.attrib['name'], a.attrib.get('interface', '?'))
        for a in m.findall('arg')
        if a.attrib['type'] == 'object'
    ]
    constructed = wlu.GetConstructedInterface(m)
    if constructed:
      constructed_interfaces.add(constructed)
      dep_graph[constructed] = ToNode(i, m)
  for _, i in wlu.AllInterfaces(protocols):
    if i.attrib['name'] not in constructed_interfaces:
      dep_graph[i.attrib['name']] = ('wl_registry', 'bind')
  return dep_graph


class SequenceContext(object):
  """Store for data used when building one sequence.

  This class is used to store the data which will help build the sequences but
  which does not actually appear in the sequences.  You should make a new
  SequenceContext for every sequence you want to generate.
  """

  def __init__(self, dep_graph):
    self.counts = {}
    self.prevented = set()
    self.dep_graph = dep_graph
    # To simulate what the harness itself does, we make a singleton wl_display.
    self.RecordInterfaceCreated('wl_display')
    self.Prevent('wl_display')

  def IsPrevented(self, interface_name):
    return interface_name in self.prevented

  def Prevent(self, interface_name):
    self.prevented.add(interface_name)

  def RecordInterfaceCreated(self, interface_name):
    self.counts[interface_name] = self.counts.get(interface_name, -1) + 1

  def GetLastInterfaceCreated(self, interface_name):
    """Return the small_value index for the currently available interface.

    Args:
      interface_name: the name of the interface you want an index of.

    Returns:
      A small_value index for the topmost-version of the interface.
    """
    return small_value[self.counts[interface_name]]


def GetSequenceForInterface(i_name, context):
  if context.IsPrevented(i_name):
    return []
  if i_name == 'wl_registry' or context.dep_graph[i_name] == ('wl_registry',
                                                              'bind'):
    context.Prevent(i_name)
  (cons_i, cons_m) = context.dep_graph[i_name]
  seq = GetSequenceForMessage(cons_i, cons_m, context, i_name)
  context.RecordInterfaceCreated(i_name)
  return seq


def GetSequenceForMessage(i_name, m_name, context, target_i):
  """Return the message sequence up to and including the supplied message.

  Args:
    i_name: the name of the interface that defines the message you want to
      send.
    m_name: the name of the message you want to send.
    context: the global context state,
    target_i: the interface you expect to be created by this message (if there
      is one, otherwise use '').

  Returns:
    A list of (i, m, [args...]) tuples, each having an interface name, message
    name, and arguments for invoking that message. The ('', '', []) tuple
    indicates the default message (which sends wl_display_roundtrip()).
  """
  seq = []
  args = []
  # get the message sequence needed to create each argument for the target
  # message.
  for arg_n, arg_t in context.dep_graph[(i_name, m_name)]:
    seq += GetSequenceForInterface(arg_t, context)
    args.append((arg_n, context.GetLastInterfaceCreated(arg_t)))
  if i_name == 'wl_registry' and m_name == 'bind' and target_i:
    args.append(('global', target_i))
  seq.append((i_name, m_name, args))
  # We need to do a round-trip after binding the registry so that we have the
  # globals available for binding.
  if i_name == 'wl_display' and m_name == 'get_registry' and target_i:
    seq.append(round_trip)
  return seq


def Main(argv):
  """Instantiate the group of message-sequences used to seed the fuzzer.

  Args:
    argv: command-line arguments used to run the sequencer.
  """
  parsed = wlu.ParseOpts(argv)
  out_dir = parsed.output
  if not os.path.exists(out_dir):
    os.mkdir(out_dir)

  protocols = wlu.ReadProtocols(parsed.spec)
  dep_graph = GetDependencyGraph(protocols)
  for _, interface in wlu.AllInterfaces(protocols):
    for req in interface.findall('request'):
      interface_name = interface.attrib['name']
      message_name = req.attrib['name']
      sequence = GetSequenceForMessage(interface_name, message_name,
                                       SequenceContext(dep_graph), '')
      # Add a round-trip to the sequence in case the server wants to do
      # something funky.
      sequence += [round_trip]

      out_path = os.path.join(out_dir,
                              '%s_%s.asciipb' % (interface_name, message_name))
      wayland_templater.InstantiateTemplate(
          parsed.input, {'sequence': sequence}, out_path, parsed.directory)


if __name__ == '__main__':
  Main(sys.argv)
