# Simple Lists in MVC Land

### Overview
This tutorial is intended to go over a basic implementation of lists in the Chrome on Android MVC
framework. If you're not sure what MVC is, see [Additional Resources](#Additional-Resources).

In this example we'll be creating a simple menu list where each list item consists of an icon and
label.

### Additional Resources
[Introductory MVC tutorial][mvc_tutorial_link]

#### Todo
[Update this doc to use data providers](https://crbug.com/967054)

### File Structure
The file structure of our component will be the following:
* ./chrome/android/java/src/org/chromium/chrome/browser/simple_menu/
  * [`SimpleMenuCoordinator.java`](#SimpleMenuCoordinator)
  * [`SimpleMenuItemViewBinder.java`](#SimpleMenuItemViewBinder)
  * [`SimpleMenuProperties.java`](#SimpleMenuProperties)
* ./chrome/android/java/res/layout/
  * [`simple_menu_item.xml`](#simple_menu_item_xml)

### SimpleMenuCoordinator
This class will own the ListAdapter that knows how to show PropertyModels. In this example we'll be
combining the responsibilities of what would otherwise be the coordinator and mediator for
simplicity.

```java
public class SimpleMenuCoordinator {

    public SimpleMenuCoordinator(Context context, ListView listView) {
        ModelListAdapter adapter = new ModelListAdapter();

        final LayoutInflater layoutInflater = context.getSystemService(LAYOUT_INFLATER_SERVICE);

        // If this is a heterogeneous list, register more than one type.
        adapter.registerType(
                ListItemType.DEFAULT,
                () -> layoutInflater.inflate(R.layout.simple_menu_item, null),
                SimpleMenuItemViewBinder::bind);

        listView.setAdapter(adapter);

        List<Pair<Integer, PropertyModel>> items = new ArrayList<>();
        PropertyModel listModel1 = generateListItem(
                ApiCompatibilityUtils.getDrawable(context.getResources(), R.drawable.icon),
                context.getResources().getString(R.string.label));
        // The list adapter needs to be told what kind of view should render the data; hence the
        // pair object.
        list.add(new Pair(ListItemType.DEFAULT, listModel1));

        // ... add other list items as needed. Typically this work is done in the mediator piece of
        // the component.

        adapter.updateModels(items);
    }

    public PropertyModel generateListItem(Drawable icon, String text) {
        return PropertyModel.Builder(SimpleMenuProperties.ALL_KEYS)
                .with(SimpleMenuProperties.ICON, icon)
                .with(SimpleMenuProperties.LABEL, text)
                .with(SimpleMenuProperties.CLICK_LISTENER, (view) -> handleClick(view))
                .build();
        // Click handling can be done as above or the listener can be passed in.
    }

    private void handleClick(View view) {
        // Do some click logic here. This would typically be done in the mediator.
    }
}
```

### SimpleMenuProperties
These are the types of data that we want to apply to each list item in our menu.
```java
class SimpleMenuProperties {
    @IntDef({ListItemType.DEFAULT})
    @Retention(RetentionPolicy.SOURCE)
    /**
     * This can be one or more items depending on if the list is homogeneous. If homogeneous,
     * this definition can be skipped and 0 can be used in place of that parameter.
     */
    public @interface ListItemType {
        int DEFAULT = 0;
    }

    /** The icon for the list item. */
    public static final WritableObjectPropertyKey<Drawable> ICON =
            new WritableObjectPropertyKey<>();

    /** The text shown next to the icon. */
    public static final WritableObjectPropertyKey<String> LABEL =
            new WritableObjectPropertyKey<>();

    /** The action that occurs when the list item is tapped. */
    public static final WritableObjectPropertyKey<OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {ICON, LABEL, CLICK_LISTENER};
}
```

### SimpleMenuItemViewBinder
As per the MVC architecture, this class is responsible for taking a model and applying that
information in to to a provided view.
```java
class SimpleMenuItemViewBinder {

    // This can optionally be in the coordinator file depending on the complexity.
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (SimpleMenuProperties.ICON == propertyKey) {
            ((ImageView) view.findViewById(R.id.simple_menu_icon)).setImageDrawable(
                    model.get(SimpleMenuProperties.ICON));

        } else if (SimpleMenuProperties.LABEL == propertyKey) {
            ((TextView) view.findViewById(R.id.simple_menu_label)).setText(
                    model.get(SimpleMenuProperties.LABEL));

        } else if (SimpleMenuProperties.CLICK_LISTENER == propertyKey) {
            view.setOnClickListener(model.get(SimpleMenuProperties.CLICK_LISTENER));
        }
    }

}
```

### simple_menu_item.xml
```xml
<?xml version="1.0" encoding="utf-8"?>
<!-- Copyright 2019 The Chromium Authors. All rights reserved.
     Use of this source code is governed by a BSD-style license that can be
     found in the LICENSE file. -->
<LinearLayout
    xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="wrap_content"
    android:orientation="horizontal"
    android:background="@color/modern_primary_color">

    <org.chromium.ui.widget.ChromeImageView
        android:id="@+id/simple_menu_icon"
        android:layout_width="18dp"
        android:layout_height="18dp"
        android:layout_gravity="center_vertical"
        android:scaleType="centerInside"/>

    <TextView
        android:id="@+id/simple_menu_label"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:gravity="center_vertical"
        android:textAppearance="@style/TextAppearance.BlackBody"/>

</LinearLayout>
```

[mvc_tutorial_link]:https://chromium.googlesource.com/chromium/src/+/HEAD/docs/ui/android/mvc_architecture_tutorial.md

