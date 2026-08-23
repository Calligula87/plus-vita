# Vita public Apollo hardware test

Date: 2026-08-23

## Scope

Credential-free, read-only traversal of Sony's public Apollo catalogue for a
Swiss account region (`CH`, mapped by Pylux to its European catalogue group).
No token, cookie, account identifier, POST request, or PC relay was used.

## Transport result

The Vita system HTTPS implementation failed during `sceHttpSendRequest` with
`SCE_HTTP_ERROR_SSL` (`0x80431075`) before receiving an HTTP status. Changing
the native HTTP template version and increasing its memory pools did not alter
that result.

The catalogue adapter was therefore moved to the official VitaSDK
`curl-mbedtls` package. Peer and hostname verification remain enabled, using a
packaged Mozilla-derived CA bundle from curl. The transport runs entirely on
the Vita.

## Physical result

- Result: PASS
- Checks: 78
- Failures: 0
- Region: CH/EU
- Traversal complete: yes
- Unique titles: 781
- HTTPS requests: 36
- Final HTTP status: 200

This establishes that the Vita can fetch and parse the real public European
catalogue directly. It does not establish authenticated catalogue access,
entitlement ownership, session allocation, media decoding, or controller
transport; those remain separate gates.
