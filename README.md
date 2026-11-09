# Native Discord bridge

C++/Qt RetroShare GUI plugin. No Python, external bridge process, webhook server,
or additional listening port. One Discord text channel maps to one **already
joined RetroShare lobby**. Open the plugin's configuration panel to start it.

## Build

Requires CMake 3.22+, Qt 5.15 Core, Widgets and Network, a C++17 compiler, and the matching
RetroShare source tree. On Windows, build `libretroshare` first. Always use the
same architecture, Qt major version, compiler ABI and RetroShare revision as
the host application.

From this directory, using the MSYS2 MinGW64 toolchain on Windows:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 2
ctest --test-dir build --output-on-failure
```

The plugin is `build/libDiscord.dll` on Windows (`libDiscord.so` on Linux).
Tests require Qt Test and the built RetroShare library (including its runtime DLLs;
override `RETROSHARE_RUNTIME_DIRS` if their directories differ); pass
`-DDISCORD_BUILD_TESTS=OFF` to build only the plugin. Linux can use the default
CMake generator instead. A qmake project, `Discord.pro`, is also provided for
the existing RetroShare build layout, but the standalone CMake build is the
validated build here. The parent repository build files are not modified.

Copy the plugin into a plugin directory listed by your RetroShare installation,
restart RetroShare, enable it under Settings → Plugins, and open its configuration
panel. Build products have not been installed automatically.

## Discord setup and use

1. Create an application and bot in the [Discord Developer Portal](https://discord.com/developers/applications).
2. Enable **Message Content Intent** in the bot settings (approval may be required
   by Discord). Invite the bot to your server with **View Channel**, **Read Message
   History**, and **Send Messages** permissions for the selected text channel.
3. Enable Discord Developer Mode and copy the text channel ID.
4. Join the destination lobby in RetroShare. Tell its participants that messages
   will be forwarded to Discord.
5. In the plugin panel, enter the bot token and channel ID, select the joined
   lobby, and press **Start bridge**. The status label reports connection errors.
   Use only one active bridge for a given room/channel pair.

## Behavior and limits

- HTTPS REST polling, normally one request every two seconds. Polling and sending
  alternate while outbound messages are queued; expect roughly 2–4 seconds of
  latency without a backlog. Discord rate limits can increase this delay.
- The first successful fetch establishes a baseline and skips existing Discord
  history. Subsequent polls fetch up to 100 messages after the cursor, in numeric
  ID order. Stop/restart discards pending outbound messages and starts a new baseline.
- Text only: no attachments, edits, deletions, reactions, threads, voice or history
  synchronization. Empty-content messages are skipped. The bot may appear offline
  in Discord because this version does not open a Gateway connection.
- Discord bot and webhook messages are ignored. Outgoing RetroShare echoes are
  suppressed while locally typed messages are forwarded. Other lobby participants'
  names use the supplied nickname, falling back to their RetroShare identity ID.
- RetroShare rich text becomes plain text on Discord. Discord text is HTML-escaped
  before sending to RetroShare. Discord mentions are disabled on forwarded messages.
- Outbound messages split at 2,000 UTF-16 units without splitting surrogate pairs.
  The queue is bounded to 200 chunks; overflow drops the newest message with a
  status notice. An ambiguous POST failure drops that chunk to avoid a duplicate;
  this is a best-effort bridge, not guaranteed delivery.
- HTTP 429 honors retry delays; other temporary failures back off to 60 seconds.
  Authentication/permission/not-found errors stop the bridge. Leaving the selected
  lobby also stops it. Tokens and HTTP response bodies are not logged.
- Uses Qt's normal certificate verification and system proxy configuration.


Protocol references: [Discord messages](https://docs.discord.com/developers/resources/message)
and [rate limits](https://docs.discord.com/developers/topics/rate-limits).

License: AGPL-3.0-or-later.
