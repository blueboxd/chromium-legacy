# What is RenderDocument?

## TL;DR

RenderDocument stops us reusing RenderFrames and RenderFrameHosts,
simplifying decisions,
eliminating logic for reuse
and making RenderFrameHost a browser-side object
that corresponds to the renderer-side document
(hence RenderDocument).

## Details

Previously when we navigate a frame from one page to another,
the second page may appear in a new RenderFrame
or we may reuse the existing RenderFrame to load the second page.
Which happens depends on many things,
including which site-isolation policy we are following
and whether the pages are from the same site or not.
With RenderDocument,
the second page will always use a new RenderFrame
(excluding navigation within a document).

Also when reloading a crashed frame
we reused the browser-side RenderFrameHost.
With RenderDocument we create a new RenderFrameHost
for crashed frames.

## Read more

https://crbug.com/936696

[high-level view of the work needed](https://docs.google.com/document/d/1UzVOmTj2IJ0ecz7CZicTK6ow2rr9wgLTGfY5hjyLmT4)

[discussion of how we can land it safely](https://docs.google.com/document/d/1ZHWWEYT1L5Zgh2lpC7DHXXZjKcptI877KKOqjqxE2Ns)

# Stages

We have 3 stages that are behind flags

1. crashed-frames:
  With this enabled we get a new RenderFrameHost when reloading a crashed frame.
2. subframes:
  With this enabled,
  when a subframe navigates
  we get a new RenderFrame and RenderFrameHost.
3. main frames:
  With this enabled,
  when the main frame navigates
  we get a new RenderFrame and RenderFrameHost.

# Test changes

## RenderFrameHost reference becomes invalid

Enabling this for subframes and main frames causes many tests to fail.
It is common for tests to get a reference to a RenderFrameHost
and then navigate that frame,
assuming that the reference will remain valid.
This assumption is no longer valid.
The test needs to get a reference to the new RenderFrameHost,
e.g. by traversing the frame tree again.
