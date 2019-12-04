#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs tests with Xvfb and Openbox on Linux and normally on other platforms."""

import os
import os.path
import random
import re
import signal
import subprocess
import sys
import threading
import time
import test_env


class _XvfbProcessError(Exception):
  """Exception raised when Xvfb cannot start."""
  pass


def kill(proc, name, timeout_in_seconds=10):
  """Tries to kill |proc| gracefully with a timeout for each signal."""
  if not proc:
    return

  proc.terminate()
  thread = threading.Thread(target=proc.wait)
  thread.start()

  thread.join(timeout_in_seconds)
  if thread.is_alive():
    print >> sys.stderr, '%s running after SIGTERM, trying SIGKILL.' % name
    proc.kill()

  thread.join(timeout_in_seconds)
  if thread.is_alive():
    print >> sys.stderr, \
      '%s running after SIGTERM and SIGKILL; good luck!' % name


def launch_dbus(env):
  """Starts a DBus session.

  Works around a bug in GLib where it performs operations which aren't
  async-signal-safe (in particular, memory allocations) between fork and exec
  when it spawns subprocesses. This causes threads inside Chrome's browser and
  utility processes to get stuck, and this harness to hang waiting for those
  processes, which will never terminate. This doesn't happen on users'
  machines, because they have an active desktop session and the
  DBUS_SESSION_BUS_ADDRESS environment variable set, but it can happen on
  headless environments. This is fixed by glib commit [1], but this workaround
  will be necessary until the fix rolls into Chromium's CI.

  [1] f2917459f745bebf931bccd5cc2c33aa81ef4d12

  Modifies the passed in environment with at least DBUS_SESSION_BUS_ADDRESS and
  DBUS_SESSION_BUS_PID set.

  Returns the pid of the dbus-daemon if started, or None otherwise.
  """
  if 'DBUS_SESSION_BUS_ADDRESS' in os.environ:
    return
  try:
    dbus_output = subprocess.check_output(['dbus-launch'], env=env).split('\n')
    for line in dbus_output:
      m = re.match(r'([^=]+)\=(.+)', line)
      if m:
        env[m.group(1)] = m.group(2)
    return int(env['DBUS_SESSION_BUS_PID'])
  except (subprocess.CalledProcessError, OSError, KeyError, ValueError) as e:
    print 'Exception while running dbus_launch: %s' % e


# TODO(crbug.com/949194): Encourage setting flags to False.
def run_executable(
    cmd, env, stdoutfile=None, use_openbox=True, use_xcompmgr=True):
  """Runs an executable within Xvfb on Linux or normally on other platforms.

  The method sets SIGUSR1 handler for Xvfb to return SIGUSR1
  when it is ready for connections.
  https://www.x.org/archive/X11R7.5/doc/man/man1/Xserver.1.html under Signals.

  Args:
    cmd: Command to be executed.
    env: A copy of environment variables, "DISPLAY" and
      "_CHROMIUM_INSIDE_XVFB" will be set if Xvfb is used.
    stdoutfile: If provided, symbolization via script is disabled and stdout
      is written to this file as well as to stdout.
    use_openbox: A flag to use openbox process.
      Some ChromeOS tests need a window manager.
    use_xcompmgr: A flag to use xcompmgr process.
      Some tests need a compositing wm to make use of transparent visuals.

  Returns:
    the exit code of the specified commandline, or 1 on failure.
  """

  # It might seem counterintuitive to support a --no-xvfb flag in a script
  # whose only job is to start xvfb, but doing so allows us to consolidate
  # the logic in the layers of buildbot scripts so that we *always* use
  # xvfb by default and don't have to worry about the distinction, it
  # can remain solely under the control of the test invocation itself.
  use_xvfb = True
  if '--no-xvfb' in cmd:
    use_xvfb = False
    cmd.remove('--no-xvfb')

  # Tests that run on Linux platforms with Ozone/Wayland backend require
  # a Weston instance. However, there is no solution to run pure headless
  # Wayland at the moment. Instead, the Weston compositor runs on top of
  # X Server, which is ok, because Weston does all the communication job
  # internally and clients are using Wayland protocols normally and unaware
  # of the X.
  use_weston = False
  if '--use-weston' in cmd:
    if not use_xvfb:
      print >> sys.stderr, 'Unable to use Weston without xvfb'
      return 1
    use_weston = True
    cmd.remove('--use-weston')

  if sys.platform == 'linux2' and use_xvfb:
    env['_CHROMIUM_INSIDE_XVFB'] = '1'
    openbox_proc = None
    xcompmgr_proc = None
    weston_proc = None
    xvfb_proc = None
    xvfb_ready = MutableBoolean()
    def set_xvfb_ready(*_):
      xvfb_ready.setvalue(True)

    try:
      signal.signal(signal.SIGTERM, raise_xvfb_error)
      signal.signal(signal.SIGINT, raise_xvfb_error)

      # Due to race condition for display number, Xvfb might fail to run.
      # If it does fail, try again up to 10 times, similarly to xvfb-run.
      for _ in range(10):
        xvfb_ready.setvalue(False)
        display = find_display()

        # Sets SIGUSR1 to ignore for Xvfb to signal current process
        # when it is ready. Due to race condition, USR1 signal could be sent
        # before the process resets the signal handler, we cannot rely on
        # signal handler to change on time.
        signal.signal(signal.SIGUSR1, signal.SIG_IGN)
        xvfb_proc = subprocess.Popen(
            ['Xvfb', display, '-screen', '0', '1280x800x24', '-ac',
             '-nolisten', 'tcp', '-dpi', '96', '+extension', 'RANDR'],
            stderr=subprocess.STDOUT, env=env)
        signal.signal(signal.SIGUSR1, set_xvfb_ready)
        for _ in range(10):
          time.sleep(.1)  # gives Xvfb time to start or fail.
          if xvfb_ready.getvalue() or xvfb_proc.poll() is not None:
            break  # xvfb sent ready signal, or already failed and stopped.

        if xvfb_proc.poll() is None:
          break  # xvfb is running, can proceed.
      if xvfb_proc.poll() is not None:
        raise _XvfbProcessError('Failed to start after 10 tries')

      env['DISPLAY'] = display

      dbus_pid = launch_dbus(env)

      if use_openbox:
        openbox_proc = subprocess.Popen(
            'openbox', stderr=subprocess.STDOUT, env=env)

      if use_xcompmgr:
        xcompmgr_proc = subprocess.Popen(
            'xcompmgr', stderr=subprocess.STDOUT, env=env)

      if use_weston:
        weston_proc = subprocess.Popen(
            'weston', stderr=subprocess.STDOUT, env=env)

      return test_env.run_executable(cmd, env, stdoutfile)
    except OSError as e:
      print >> sys.stderr, 'Failed to start Xvfb or Openbox: %s' % str(e)
      return 1
    except _XvfbProcessError as e:
      print >> sys.stderr, 'Xvfb fail: %s' % str(e)
      return 1
    finally:
      kill(openbox_proc, 'openbox')
      kill(xcompmgr_proc, 'xcompmgr')
      kill(weston_proc, 'weston')
      kill(xvfb_proc, 'Xvfb')

      # dbus-daemon is not a subprocess, so we can't SIGTERM+waitpid() on it.
      # To ensure it exits, use SIGKILL which should be safe since all other
      # processes that it would have been servicing have exited.
      if dbus_pid:
        os.kill(dbus_pid, signal.SIGKILL)
  else:
    return test_env.run_executable(cmd, env, stdoutfile)


class MutableBoolean(object):
  """Simple mutable boolean class. Used to be mutated inside an handler."""

  def __init__(self):
    self._val = False

  def setvalue(self, val):
    assert isinstance(val, bool)
    self._val = val

  def getvalue(self):
    return self._val


def raise_xvfb_error(*_):
  raise _XvfbProcessError('Terminated')


def find_display():
  """Iterates through X-lock files to find an available display number.

  The lower bound follows xvfb-run standard at 99, and the upper bound
  is set to 119.

  Returns:
    A string of a random available display number for Xvfb ':{99-119}'.

  Raises:
    _XvfbProcessError: Raised when displays 99 through 119 are unavailable.
  """

  available_displays = [
      d for d in range(99, 120)
      if not os.path.isfile('/tmp/.X{}-lock'.format(d))
  ]
  if available_displays:
    return ':{}'.format(random.choice(available_displays))
  raise _XvfbProcessError('Failed to find display number')


def main():
  usage = 'Usage: xvfb.py [command [--no-xvfb or --use-weston] args...]'
  if len(sys.argv) < 2:
    print >> sys.stderr, usage
    return 2

  # If the user still thinks the first argument is the execution directory then
  # print a friendly error message and quit.
  if os.path.isdir(sys.argv[1]):
    print >> sys.stderr, (
        'Invalid command: \"%s\" is a directory' % sys.argv[1])
    print >> sys.stderr, usage
    return 3

  return run_executable(sys.argv[1:], os.environ.copy())


if __name__ == '__main__':
  sys.exit(main())
