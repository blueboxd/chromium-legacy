# Commander overview

This directory contains the bulk of the Commander, a new UI surface that allows
users to access most Chromium functionality via a keyboard interface.

## Components

The three main components of the commander are:

### View

The view is responsible for passing user input to the controller and displaying
the subsequent results, delivered in a `CommanderViewModel`. The implementation
will most likely be WebUI hosted in a Views toolkit widget.

### Command Sources

Command sources (`CommandSource`) are responsible for providing a list of
possible commands (`CommandItem`) for the user's current context, then
executing them if they are chosen.

### Controller

The controller (`CommanderController`) is responsible for maintaining a
list of command sources, passing user input to them, and sorting and ranking
the result.

If a command requires more input (for example, choosing a window to move the
current tab to), the command source will provide a delegate which implements
the same `CommanderBackend` interface as the controller. The controller then
mediates between the delegate and the view.

## Relationships

To sum up the relationships of the classes in this file:

`CommandController` (a singleton), owns multiple `CommandSource`s. On user
input, each `CommandSource` provides the controller with matching
`CommandItems`. The controller sorts, ranks and processes the `CommandItems`,
producing a `CommanderViewModel` and sending it to the view.

When the controller receives a message indicating that the user has chosen an
option, the `CommandItem` is executed if it's "one-shot". Otherwise, it's
a composite command and a delegate `CommanderBackend` is created. The controller
mediates between the delegate and view, passing input to the delegate and view
models to the view until the command is either completed or cancelled.
