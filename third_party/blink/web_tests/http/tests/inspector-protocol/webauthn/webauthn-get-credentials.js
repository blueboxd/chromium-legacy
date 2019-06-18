(async function(testRunner) {
  const {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/webauthn-test.https.html",
          "Check that the WebAuthn command getCredentials works");

  await dp.WebAuthn.enable();
  const authenticatorId = (await dp.WebAuthn.addVirtualAuthenticator({
    options: {
      protocol: "ctap2",
      transport: "usb",
      hasResidentKey: false,
      hasUserVerification: false,
    },
  })).result.authenticatorId;

  // No credentials registered yet.
  testRunner.log(await dp.WebAuthn.getCredentials({authenticatorId}));

  // Register two credentials.
  testRunner.log((await session.evaluateAsync("registerCredential()")).status);
  testRunner.log((await session.evaluateAsync("registerCredential()")).status);

  // Get the registered credentials.
  let credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  let expectedRpIdHash = await session.evaluateAsync("generateRpIdHash()");
  for (let credential of credentials) {
    if (credential.rpIdHash === expectedRpIdHash)
      testRunner.log("RP ID hash matches expected value");
    else
      testRunner.log(`RP ID hash does not match. Actual: ${credential.rpIdHash}, expected: ${expectedRpIdHash}`);
    testRunner.log(credential.signCount);
  }

  // Authenticating with the first credential should succeed.
  let credential = credentials[0];
  testRunner.log(await session.evaluateAsync(`getCredential({
    type: "public-key",
    id: base64ToArrayBuffer("${credential.credentialId}"),
    transports: ["usb", "ble", "nfc"],
  })`));

  // Sign count should be increased by one for |credential|.
  credentials = (await dp.WebAuthn.getCredentials({authenticatorId})).result.credentials;
  testRunner.log(credentials.find(
      cred => cred.id === credential.id).signCount);

  // We should be able to parse the private key.
  let keyData =
      Uint8Array.from(atob(credential.privateKey), c => c.charCodeAt(0)).buffer;
  let key = await window.crypto.subtle.importKey(
      "pkcs8", keyData, { name: "ECDSA", namedCurve: "P-256" },
      true /* extractable */, ["sign"]);

  testRunner.log(key);

  testRunner.completeTest();
})
