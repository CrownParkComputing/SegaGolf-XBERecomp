# English interface

The first English pass contains 66 messages and 243 sprite labels. English is
on by default. In either player window, open F10 and toggle **English text and
labels** to compare it with Japanese. To start in Japanese:

```sh
SEGAGOLF_LANGUAGE=ja ./run.sh
```

This covers card entry, play-mode choices, multiplayer prompts, common dialog
buttons, tutorial controls, club/camera/aim/spin labels, selected shot feedback
and general caddie warnings. Course-specific caddie commentary, character names,
operator screens, voice recordings and some HUD artwork remain Japanese.

The game uses two text systems. Runtime messages are Shift-JIS strings in the
XBE. Other labels are drawn from XST sprite atlases, often packed as SGGZ plus
gzip with embedded XPR0/DXT5 textures. Font sheets are also packed textures.

`en.json` contains only English wording and identifiers. During a build,
`tools/build_localization.py` reads the owner's verified local game files and
generates private metadata under `generated/game/`. It checks string capacity
and printf argument types. At startup the port verifies each original string's
fingerprint before replacing it in guest memory, and retains the original bytes
for the language toggle. Neither the XBE nor the extracted assets are modified.

For image labels, the native renderer recognizes the atlas page and sprite UV
rectangle, then draws an English label at the original quad's current screen
position. The original touch regions and game logic are retained. This first
pass uses simple text panels rather than reproducing the Japanese typography.
The observations do not change the game's GPU commands or textures.

Native F3 captures save the guest image before host overlays, so they show
English runtime messages but retain Japanese sprite artwork underneath. Use a
desktop screenshot to capture the complete visible English interface.

No original or modified game images are distributed. To extend the translation,
edit `en.json` and rebuild. String entries use guest virtual addresses; sprite
entries use XST filenames and zero-based sprite indices. Build validation rejects
strings that exceed their original slot or change the formatting arguments.
An optional normalized `box` limits a label to part of a sprite, preserving live
numbers drawn over the rest of a combined HUD graphic.
