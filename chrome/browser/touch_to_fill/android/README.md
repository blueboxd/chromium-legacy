# Touch To Fill Android Feature

This folder contains the Android UI implementation for the Touch To Fill
feature. Touch To Fill provides users with a trusted surface to authorize
transactions, such as filling their credentials.

[TOC]

## Use case

This component displays a set of saved credentials. The user selects one which
is then filled into the form. If the user dismisses the sheet, the keyboard
will be shown instead (i.e. by changing the focus).


## Folder Structure

#### /

The root folder contains the public interface of this component and data that is
used to fill it with content, e.g. Crendentials. This folder also contains the
code to instantiate the component.

## Example usage

``` java

// Use factory to instantiate the component:
TouchToFillComponent component = TouchToFillComponentFactory.createComponent();

component.initialize(activity, activity.getBottomSheetController(), () -> {
  // Things to do when the component is dismissed.
}));

List<Credential> creds; // Add credentials to show!
component.showCredentials("www.displayed-url.xzy", creds, (credential) -> {
  // The |credential| that was clicked should be used to fill something now.
})

```
