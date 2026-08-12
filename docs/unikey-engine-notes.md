# UniKey engine notes for the Rust core

Reference studied: <https://github.com/fcitx/fcitx5-unikey/tree/master>.

VietIME is an independent Rust implementation. It does not link or copy the
UniKey C/C++ engine. The reference was used to understand observable input
method behavior and its state-machine boundaries.

## Behaviors adopted

- Keep raw keystrokes separately from rendered Unicode text.
- Treat tone, roof, horn/breve and `dd` as events, not literal replacement.
- Repeating the active tone removes it and emits the repeated key literally.
- Repeating an existing roof/horn/`dd` restores the base and emits the key.
- Backspace removes one raw keystroke and renders the remaining state again.
- Recompute tone position whenever the vowel nucleus or coda changes.
- Handle `ươ`, `qu` and `gi` as special nucleus/glide cases.
- Keep one engine per Fcitx5 input context.
- At a word boundary, restore raw keystrokes when a converted word cannot be
  parsed as onset + contiguous vowel nucleus + legal coda.

## Deliberate VietIME differences

- Output is NFC Unicode only.
- The adapter commits literal characters immediately and applies Telex diffs
  only through valid surrounding text. It deliberately displays no preedit.
- All terminal contexts use a hidden deferred buffer and commit only at a word
  boundary, because terminal surrounding-text capability is not a reliable
  indication that shell/readline deletion works. Other clients retain direct
  replacement.
- Legacy encodings, macros, VNI and full spell-check tables are not yet part of
  the Rust core.
