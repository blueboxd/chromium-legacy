// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Checks for the reparent notify events that is a signal that a WM has been
// started. Returns 0 on success and 1 on failure. This program must be started
// BEFORE the Wm starts.
//

#include <cerrno>
#include <cstdio>

#include <time.h>

#include <X11/Xlib.h>

void CalculateTimeout(const timespec& now,
                      const timespec& deadline,
                      timeval* timeout) {
  // 1s == 1e+6 us.
  // 1nsec == 1e-3 us
  timeout->tv_usec = (deadline.tv_sec - now.tv_sec) * 1000000 +
                     (deadline.tv_nsec - now.tv_nsec) / 1000;
  timeout->tv_sec = 0;
}

class XScopedDisplay {
 public:
  explicit XScopedDisplay(Display* display) : display_(display) {}
  ~XScopedDisplay() {
    if (display_)
      XCloseDisplay(display_);
  }

  Display* display() const { return display_; }

 private:
  Display* const display_;
};

int main(int argc, char* argv[]) {
  // Connects to a display specified in the current process' env value DISPLAY.
  XScopedDisplay scoped_display(XOpenDisplay(nullptr));

  // No display found - fail early.
  if (!scoped_display.display()) {
    fprintf(stderr, "Couldn't connect to a display.\n");
    return 1;
  }

  auto* xdisplay = scoped_display.display();

  auto root_window = DefaultRootWindow(xdisplay);
  if (!root_window) {
    fprintf(stderr, "Couldn't find root window.\n");
    return 1;
  }

  auto dummmy_window = XCreateSimpleWindow(
      xdisplay, root_window, 0 /*x*/, 0 /*y*/, 1 /*width*/, 1 /*height*/,
      0 /*border width*/, 0 /*border*/, 0 /*background*/);
  if (!dummmy_window) {
    fprintf(stderr, "Couldn't create a dummy window.");
    return 1;
  }

  XMapWindow(xdisplay, dummmy_window);
  // We are only interested in the ReparentNotify events that are sent whenever
  // our dummy window is reparented because of a wm start.
  XSelectInput(xdisplay, dummmy_window, StructureNotifyMask);
  XFlush(xdisplay);

  int display_fd = ConnectionNumber(xdisplay);

  // Set deadline as 30s.
  struct timespec now, deadline;
  clock_gettime(CLOCK_REALTIME, &now);
  deadline = now;
  deadline.tv_sec += 30;

  // Calculate first timeout.
  struct timeval tv;
  CalculateTimeout(now, deadline, &tv);

  XEvent ev;
  do {
    fd_set in_fds;
    FD_ZERO(&in_fds);
    FD_SET(display_fd, &in_fds);

    int ret = select(display_fd + 1, &in_fds, nullptr, nullptr, &tv);
    if (ret == -1) {
      if (errno != EINTR) {
        perror("Error occured while polling the display fd");
        break;
      }
    } else if (ret > 0) {
      while (XPending(xdisplay)) {
        XNextEvent(xdisplay, &ev);
        // If we got ReparentNotify, a wm has started up and we can stop
        // execution.
        if (ev.type == ReparentNotify) {
          return 0;
        }
      }
    }
    // Calculate next timeout. If it's less or equal to 0, give up.
    clock_gettime(CLOCK_REALTIME, &now);
    CalculateTimeout(now, deadline, &tv);
  } while (tv.tv_usec >= 0);

  return 1;
}

#if defined(LEAK_SANITIZER)
// XOpenDisplay leaks memory if it takes more than one try to connect. This
// causes LSan bots to fail. We don't care about memory leaks in xdisplaycheck
// anyway, so just disable LSan completely.
// This function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used)) __attribute__((visibility("default"))) extern "C" int
__lsan_is_turned_off() {
  return 1;
}
#endif
