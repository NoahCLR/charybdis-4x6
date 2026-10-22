# Combo readback is device data

Cited as D-F36. Was D-L13.

A client reads the combo table the connected half actually runs (GET `0x06`),
with each combo's effective timing, rules, enable and allowed layers, instead
of inferring it from source, and a digest catches a change mid-read. Firmware
callback outputs and custom trigger and release hooks are reported as opaque,
not interpreted. Pages: [`profile-wire-v1.md`](../architecture/profile-wire-v1.md).
