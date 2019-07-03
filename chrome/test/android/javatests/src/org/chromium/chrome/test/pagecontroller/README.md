# Chrome's Page Controller Test Library

## Introduction

Page Controllers simplify the task of writing and maintaining integration tests
by organizing the logic to interact with the various application UI components
into re-usable classes.  Learn how to write and use Page Controllers to solve
your testing needs.

## File Organization

[controllers](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/controllers/): Contains all the Page Controllers.<br/>
[rules](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/rules/): Junit Test Rules that provide access to the Page Controllers in a
test case.<br/>
[tests](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/tests/): Tests for the Page Controllers themselves.<br/>
[utils](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/utils/): Utility classes that are useful for writing Page Controllers.<br/>

## Writing Testcases

See the [ExampleTest](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/tests/ExampleTest.java)

## Creating + Updating Page Controllers

Currently the Page Controllers use UIAutomator under-the-hood.  But it is
entirely possible to write methods or whole page controllers using Espresso or
some other driver framework.

```
/**
 * Page controller for CoolNewPage
 */
class CoolNewPageController extends PageController {
    // Locators allow the controller to find UI elements on the page
    // It is preferred to use Resource Ids to find elements since they are
    // stable across minor UI changes.
    private static final IUi2Locator LOCATOR_COOL_PAGE = Ui2Locators.withResIds("cool_page");
    // Any of the resource ids in the list will result in a match.
    private static final IUi2Locator LOCATOR_COOL_BUTTON = Ui2Locators.withResIds("cool_button_v1", "cool_button_v2");

    public CoolerPageController clickButton() {
        // [UiAutomatorUtils.click](https://cs.chromium.org/chromium/src/chrome/test/android/javatests/src/org/chromium/chrome/test/pagecontroller/utils/UiAutomatorUtils.java?q=click) operates on UI elements via IUi2Locators.
        // In general, methods that operate on IUi2Locators will throw if that
        // locator could not find an UI elements on the page.
        // UiAutomatorUtils has retry functionality with a configurable timeout,
        // so that flakiness can be drastically reduced.
        mUtils.click(LOCATOR_SOME_BUTTON);

        // If clicking on cool button should always result in the app going
        // to the EvenCoolerPage, then the action method should return an
        // instance of that controller via its assertIsCurrentPage() method.
        // This ensures the UI state is synced upon the return of the method.
        return EvenCoolerPageController.getInstance().assertIsCurrentPage();
    }

    // All page controllers must implement this method.
    @Override
    public SomePageController assertIsCurrentPage() {
        mLocatorHelper.get(LOCATOR_SOME_PAGE);  // Throws if not found
        return this;
    }
}
```
