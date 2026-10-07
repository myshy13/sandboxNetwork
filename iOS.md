# Running the client on an iPhone / iPad

Written for someone new to Xcode signing. Plan and design: the "iOS client" section of `plan.md`.

## What you need

- A Mac with Xcode (the fork's project is `projects/Xcode26`, so Xcode 26 or newer).
- An Apple ID. A free one is enough for your own device.
- The device and a USB cable (Wi-Fi debugging works after the first cable connection).
- The simulator needs no signing at all. Start there.

## Why signing exists

iOS only runs apps that are signed by a developer Apple knows, and only on devices that developer registered. Xcode can do
all of it for you ("automatic signing"): it makes a certificate, registers your device, and creates a provisioning profile
(the file that says "this app, signed by this person, may run on these devices").

## Steps (free Apple ID)

1. Xcode > Settings > Accounts > `+` > Apple ID, sign in. A "Personal Team" appears.
2. Plug in the device, unlock it, tap **Trust**. On iOS 16+ also turn on Settings > Privacy & Security > **Developer Mode**
   (the phone restarts).
3. Open the Xcode project, select the app target > Signing & Capabilities. Tick **Automatically manage signing** and pick
   your Personal Team.
4. Change the **Bundle Identifier** to something unique to you (e.g. `com.yourname.sandboxnetwork`). Apple refuses an
   identifier another account already holds.
5. Pick your device in the toolbar and press Run.
6. First launch only: the phone says "Untrusted Developer". Settings > General > VPN & Device Management > your Apple ID >
   **Trust**, then launch again.

## Limits of a free account

- The app stops launching after **7 days**; press Run again to re-sign it.
- At most 3 apps and a handful of new App IDs per week.
- No TestFlight or App Store. That needs the paid Apple Developer Program.

## What goes in git

Safe to commit: source, the Xcode project file, `Info.plist`, and a **template** with placeholder values.

Never commit:

- Certificates and private keys (`*.p12`, `*.cer`, `*.pem`). A private key lets anyone sign as you.
- Provisioning profiles (`*.mobileprovision`). They list your device IDs.
- Your own Team ID and bundle identifier, as a courtesy. They aren't secret, but they are personal, and committing them
  makes everyone else's build fail or fight over your App ID.

The pattern is the same as `Client/src/env.hpp` / `env.example.hpp`: keep your values in a git-ignored
`Client/ios/Signing.xcconfig`, copied from the committed `Client/ios/Signing.example.xcconfig`:

```
DEVELOPMENT_TEAM = XXXXXXXXXX
PRODUCT_BUNDLE_IDENTIFIER = com.yourname.sandboxnetwork
```

In Xcode, set the project's configuration file to `Signing.xcconfig` and leave the target's own signing fields empty so
the file wins. Find your Team ID in Xcode > Settings > Accounts (select the team > the ID is in the details), or on
developer.apple.com > Membership.

Automatic signing keeps the certificate in your login Keychain, outside the repo, so nothing sensitive reaches the folder
by default. The ignore rules are a safety net.

## Common errors

- **"No account for team"**: step 1 was skipped, or the Xcode project still has someone else's Team ID.
- **"Failed to register bundle identifier"**: it is taken; change it (step 4).
- **"Developer Mode disabled"**: step 2.
- **"Could not find Developer Disk Image" / device not listed**: update Xcode or the phone's iOS; re-plug and unlock it.
- **The app opens, then quits straight away**: look at the Xcode console. A missing `assets/` bundle resource is the usual
  cause.
- **Can't connect to the server**: the phone can't use `localhost` (see `plan.md` step 7), and it must be on the same
  network and have allowed Local Network access.
