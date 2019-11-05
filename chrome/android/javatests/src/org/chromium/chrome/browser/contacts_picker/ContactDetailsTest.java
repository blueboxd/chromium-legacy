// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contacts_picker;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.payments.mojom.PaymentAddress;

import java.util.Arrays;
import java.util.List;

/**
 * Tests for the ContactDetails class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class ContactDetailsTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    Context mContext;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getInstrumentation()
                           .getTargetContext()
                           .getApplicationContext();
    }

    private void compareAbbreviatedContactDetails(ContactDetails.AbbreviatedContactDetails expected,
            ContactDetails.AbbreviatedContactDetails actual) {
        Assert.assertEquals(expected.primaryEmail, actual.primaryEmail);
        Assert.assertEquals(expected.overflowEmailCount, actual.overflowEmailCount);
        Assert.assertEquals(expected.primaryTelephoneNumber, actual.primaryTelephoneNumber);
        Assert.assertEquals(
                expected.overflowTelephoneNumberCount, actual.overflowTelephoneNumberCount);
    }

    @Test
    @SmallTest
    public void testBasics() {
        PaymentAddress address1 = new PaymentAddress();
        address1.city = "city";
        address1.country = "country";
        address1.addressLine = new String[] {"formattedAddress1"};
        address1.postalCode = "postalCode";
        address1.region = "region";
        address1.dependentLocality = "";
        address1.sortingCode = "";
        address1.organization = "";
        address1.recipient = "";
        address1.phone = "";
        PaymentAddress address2 = new PaymentAddress();
        address2.city = "city";
        address2.country = "country";
        address2.addressLine = new String[] {"formattedAddress2"};
        address2.postalCode = "postalCode";
        address2.region = "region";
        address2.dependentLocality = "";
        address2.sortingCode = "";
        address2.organization = "";
        address2.recipient = "";
        address2.phone = "";

        ContactDetails contact = new ContactDetails("id", "Display Name",
                Arrays.asList("email@example.com", "email2@example.com"),
                Arrays.asList("555 123-4567", "555 765-4321"), Arrays.asList(address1, address2));

        Assert.assertEquals("id", contact.getId());
        Assert.assertEquals("Display Name", contact.getDisplayName());
        Assert.assertEquals("DN", contact.getDisplayNameAbbreviation());

        List<String> emails = contact.getEmails();
        Assert.assertEquals(2, emails.size());
        Assert.assertEquals("email@example.com", emails.get(0));
        Assert.assertEquals("email2@example.com", emails.get(1));

        List<String> telephones = contact.getPhoneNumbers();
        Assert.assertEquals(2, telephones.size());
        Assert.assertEquals("555 123-4567", telephones.get(0));
        Assert.assertEquals("555 765-4321", telephones.get(1));

        List<PaymentAddress> addresses = contact.getAddresses();
        Assert.assertEquals(2, addresses.size());
        Assert.assertEquals("formattedAddress1", addresses.get(0).addressLine[0]);
        Assert.assertEquals("formattedAddress2", addresses.get(1).addressLine[0]);

        Assert.assertEquals(false, contact.isSelf());
        Assert.assertEquals(null, contact.getSelfIcon());

        contact.setIsSelf(true);

        Bitmap bitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        canvas.drawColor(Color.BLUE);
        BitmapDrawable drawable = new BitmapDrawable(bitmap);
        contact.setSelfIcon(drawable);

        Assert.assertEquals(true, contact.isSelf());
        Assert.assertTrue(null != contact.getSelfIcon());

        Assert.assertEquals("", contact.getContactDetailsAsString(false, false));
        Assert.assertEquals("email@example.com\nemail2@example.com",
                contact.getContactDetailsAsString(true, false));
        Assert.assertEquals(
                "555 123-4567\n555 765-4321", contact.getContactDetailsAsString(false, true));

        Resources resources = mContext.getResources();
        ContactDetails.AbbreviatedContactDetails expected =
                new ContactDetails.AbbreviatedContactDetails();
        expected.primaryEmail = "email@example.com";
        expected.overflowEmailCount = "(+ 1 more)";
        expected.primaryTelephoneNumber = "555 123-4567";
        expected.overflowTelephoneNumberCount = "(+ 1 more)";

        // Test with full details.
        ContactDetails.AbbreviatedContactDetails actual = contact.getAbbreviatedContactDetails(
                /*includeEmails=*/true, /*includeTels=*/true, resources);
        compareAbbreviatedContactDetails(expected, actual);
        // Test with only email details.
        actual = contact.getAbbreviatedContactDetails(
                /*includeEmails=*/true, /*includeTels=*/false, resources);
        expected.primaryTelephoneNumber = "";
        expected.overflowTelephoneNumberCount = "";
        compareAbbreviatedContactDetails(expected, actual);
        // Test with no details.
        actual = contact.getAbbreviatedContactDetails(
                /*includeEmails=*/false, /*includeTels=*/false, resources);
        expected.primaryEmail = "";
        expected.overflowEmailCount = "";
        compareAbbreviatedContactDetails(expected, actual);
        // Test with only telephone details.
        actual = contact.getAbbreviatedContactDetails(
                /*includeEmails=*/false, /*includeTels=*/true, resources);
        expected.primaryTelephoneNumber = "555 123-4567";
        expected.overflowTelephoneNumberCount = "(+ 1 more)";
        compareAbbreviatedContactDetails(expected, actual);
    }
}
