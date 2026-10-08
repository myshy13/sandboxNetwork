# Bugs

1. Even if the protocol isn't synced, some of the handshake packets (e.g: `SetTime`) can be sent before the Server rejects the Client, causing one of them to crash (Most likely Client).
2. on smaller screens (iOS), you can't see the rest of the settings menus because the screen is too small and it renders outside of what the screen can see.
