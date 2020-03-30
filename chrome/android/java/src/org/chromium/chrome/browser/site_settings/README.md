# Content Settings and Site Settings in Android

[TOC]

## Overview

There are 3 main user-facing screens when it comes to displaying content
settings status to the user:

*   [Site Settings](#Site-Settings) provides a link to `All Sites` and an
    overview of all content settings. Supported by the
    [SiteSettings](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SiteSettings.java?g=0)
    class.
*   [Settings Category](#Settings-Category) displays a specific content setting
    in detail. This usually includes a toggle to enable/disable the content
    setting browser-wide, any category-specific options, and lists of sites that
    are `allowed`/`blocked`/`managed`. The same screen is used to display the
    `All Sites` category which displays all sites that have any content settings
    set. Supported by the
    [SingleCategorySettings](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleCategorySettings.java?type=cs&g=0)
    class.
*   [Website Settings](#Website-Settings) displays site-specific content
    settings information. It displays a list of content settings that the site
    is either allowed or blocked from and a button to reset all content settings
    state for this site. Supported by the
    [SingleWebsiteSettings](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?type=cs&g=0)
    class.

All of these implement `SiteSettingsPreferenceFragment` and use a layout xml to define
the preferences that form the screens.

## Site Settings

This screen is relatively static in nature, being a central hub for settings
categories.

[site_settings_preferences.xml](https://cs.chromium.org/chromium/src/chrome/android/java/res/xml/site_settings_preferences.xml?q=site_settings_preferences&sq=package:chromium&g=0&l=1)
lists all preferences.

[configurePreferences()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SiteSettings.java?type=cs&g=0&l=53)
will remove some preferences based on command line switches and feature
availability.

[updatePreferenceStates()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SiteSettings.java?type=cs&g=0&l=72)
goes through all preferences and configures cool things like: title, summary,
icon, and click listeners.

## Settings Category

This screen contains the logic used to display specific categories, as well as
the 'All sites' category."

[website_preferences.xml](https://cs.chromium.org/chromium/src/chrome/android/java/res/xml/website_preferences.xml?q=website_preferences&sq=package:chromium&g=0&l=1)
outlines the basic layout of the screen displaying the global settings for a
category.

The
[mCategory](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleCategorySettings.java?type=cs&g=0)
member holds which category needs to be displayed and is queried to dictate the
customization of the page.

[configureGlobalToggles()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleCategorySettings.java?type=cs&g=0&l=821)
handles most of the customization of the screen. It will go through all
preferences and set their values, their visibility status as well as set up
preference change listeners.

[getInfoForOrigins()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleCategorySettings.java?type=cs&q=configureGlobalToggles&g=0&l=143)
is used to fetch websites information via a
[WebsitePermissionsFetcher](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/WebsitePermissionsFetcher.java).
When the data is available it rebuilds the screen and adds the websites to the
`allowed`/`blocked`/`managed` lists.

[onPreferenceChange()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleCategorySettings.java?type=cs&g=0&l=464)
is the main listener of user input. When the user updates a preference, it will
identify the changed preference, use bridges to call the native code that
handles updating user content settings
([WebsitePreferenceBridge](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/WebsitePreferenceBridge.java)
and
[PrefServiceBridge](https://cs.chromium.org/chromium/src/chrome/browser/preferences/android/java/src/org/chromium/chrome/browser/preferences/PrefServiceBridge.java?q=PrefServiceBridge&sq=package:chromium&g=0&l=18))
and then update the screen to ensure the new preference value is taken into
account including whatever ramifications that might have.

The `All Sites` list and the three `allowed`/`blocked`/`managed` lists are
populated with the custom
[WebsitePreference](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/WebsitePreference.java)
type of preference which allows clicks to navigate to the
[SingleWebsiteSettings](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java)
fragment.

## Website Settings

This screen is used to display the relevant information for a specific website.

[single_website_preferences.xml](https://cs.chromium.org/chromium/src/chrome/android/java/res/xml/single_website_preferences.xml?q=single_website_preferences&sq=package:chromium&dr)
outlines the layout of the screen.

The underlying model object is a
[Website](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/Website.java)
object: the
[mSite](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?l=108)
member. This is either directly provided by the activity initiator, or
alternately fetched via a
[SingleWebsitePermissionsPopulator](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?l=121)
based on the website address.

[displaySitePermissions()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?l=300)
is used to configure the screen and populate it with data from
[mSite](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?l=108),
mostly by calling
[setUpPreference()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?g=0&l=332)
on every preference as well as specific custom setup functions.

[onPreferenceChange()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?g=0&l=879)
and
[onPreferenceClick()](https://cs.chromium.org/chromium/src/chrome/android/java/src/org/chromium/chrome/browser/site_settings/SingleWebsiteSettings.java?g=0&l=896)
are responsbile for listening to user input and updating the content settings
accordingly.
