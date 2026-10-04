# Bugs

1. Even if the protocol isn't synced, some of the handshake packets (e.g: `SetTime`) can be sent before the Server rejects the Client, causing one of them to crash (Most likely Client).
