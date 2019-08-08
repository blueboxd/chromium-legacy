### Update Instructions for Safety Tips Component

Safety Tips component pushes binary protos to clients via Chrome's component
updater. The proto files are stored in a Google Cloud Storage bucket
(`gs://chrome-components-safety-tips`) under versioned directories
(e.g. `gs://chrome-components-safety-tips/5/all` for version `5`, `all` for all
platforms).

Follow these instructions to write binary protos under versioned directories:

 1. Overwrite `safety_tips.asciipb` with the data. Don't forget to increment
 `version_id`.
 2. Build the binary proto: `ninja -C out/Release`. This will write the binary
proto at `out/Release/gen/chrome/browser/resources/safety_tips/safety_tips.pb`.
 3. Push the binary proto to cloud storage:
 `chrome/browser/resources/safety_tips/push_proto.py -d out/Release`
 4. Revert `safety_tips.asciipb`, update `version_id` only for future reference
 and check it in.
 5. In a day or two, navigate to chrome://components on Canary and check that
 the version of the "Safety Tips" component is updated (you might need to press
 the "Check for update" button).

 **Do not check in the full proto**.

#### In case of emergency

If the Safety Tips component needs to be disabled immediately, increment the
version number and push an empty proto.

