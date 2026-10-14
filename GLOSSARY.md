# Vita cloud streaming

Terms used to distinguish the playable stream, experimental builds and response
measurements in this project.

## Language

**Accepted stream build**:
A Vita streaming build the user has tested on hardware and accepted for picture
quality, controls, audio and stability. A successful build or host test alone
does not make a build accepted.

**Diagnostic stream build**:
A version of the accepted stream build that adds measurements while retaining
its streaming and control policies. It remains experimental until hardware
testing establishes that the instrumentation preserves the accepted experience.

**Input response latency**:
The elapsed time from a physical Vita input to the first visible game response
on the Vita screen. It includes local input handling, the cloud round trip,
game processing, video delivery and display.
_Avoid_: Ping, decode time or queue wait when referring to the whole response.

**Local submission interval**:
Elapsed local time between a recorded input or video submission boundary and a
later client boundary. It excludes unobserved cloud processing and cannot
identify the age of a displayed picture when native output has no matching
timestamp.
_Avoid_: Input response latency when reporting only client timings.
