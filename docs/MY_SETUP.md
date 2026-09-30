# Third-party apps in my setup

A few gestures in my profile send ordinary input that only becomes something
useful with an app on the computer. I use a Mac; these are the apps I pair with
the keyboard. The firmware works without them: the keys still send what they
send, the computer just does less with it.

| App | What it does for the keyboard | Used by |
| --- | --- | --- |
| [BetterMouse](https://better-mouse.com/) | Turns command-scroll into pinch-style zoom | the Pinch pointing mode (`PD_SLOT_5`) |
| [Rectangle Pro](https://rectangleapp.com/pro) | Binds mouse buttons 6 and 7 to moving and resizing the window under the pointer | the window drags: `CUSTOM_KEY_3` (Drag Window) hold and `MS_BTN3` double-tap hold |

Without BetterMouse, Pinch is still command-modified scrolling; the Zoom mode
(`PD_SLOT_3`, `Cmd+=` and `Cmd+-`) needs no app. Without Rectangle Pro the
window drags send real button 6 and button 7 presses, which most apps ignore.

The details are in [POINTER_MODES.md](POINTER_MODES.md#pinch-pd_slot_5) and
[KEYMAP.md](KEYMAP.md#window-drags).
