# Blink C++ Style Guide

This document is a list of differences from the overall [Chromium Style Guide],
which is in turn a set of differences from the [Google C++ Style Guide]. The
long-term goal is to make both Chromium and Blink style more similar to Google
style over time, so this document aims to highlight areas where Blink style
differs from Google style.

[TOC]

## May use mutable reference arguments

Mutable reference arguments are permitted in Blink, in contrast to Google style.

> Note: This rule is under [discussion](https://groups.google.com/a/chromium.org/d/msg/blink-dev/O7R4YwyPIHc/mJyEyJs-EAAJ).

**OK:**
```c++
// May be passed by mutable reference since |frame| is assumed to be non-null.
FrameLoader::FrameLoader(LocalFrame& frame)
    : frame_(&frame),
      progress_tracker_(ProgressTracker::Create(frame)) {
  // ...
}
```

## Prefer WTF types over STL and base types

See [Blink readme](../../third_party/blink/renderer/README.md#Type-dependencies)
for more details on Blink directories and their type usage.

**Good:**
```c++
  String title;
  Vector<KURL> urls;
  HashMap<int, Deque<RefPtr<SecurityOrigin>>> origins;
```

**Bad:**
```c++
  std::string title;
  std::vector<GURL> urls;
  std::unordered_map<int, std::deque<url::Origin>> origins;
```

When interacting with WTF types, use `wtf_size_t` instead of `size_t`.

## Do not use `new` and `delete`

Object lifetime should not be managed using raw `new` and `delete`. Prefer to
allocate objects instead using `std::make_unique`, `base::MakeRefCounted` or
`blink::MakeGarbageCollected`, depending on the type, and manage their lifetime
using appropriate smart pointers and handles (`std::unique_ptr`, `scoped_refptr`
and strong Blink GC references, respectively). See [How Blink Works](https://docs.google.com/document/d/1aitSOucL0VHZa9Z2vbRJSyAIsAz24kX8LFByQ5xQnUg/edit#heading=h.ekwf97my4bgf)
for more information.

[Chromium Style Guide]: c++.md
[Google C++ Style Guide]: https://google.github.io/styleguide/cppguide.html
[Chromium Documentation Guidelines]: ../../docs/documentation_guidelines.md
[Google C++ Style Guide: Comments]: https://google.github.io/styleguide/cppguide.html#Comments
[Google C++ Style Guide: TODO Comments]:https://google.github.io/styleguide/cppguide.html#TODO_Comments
