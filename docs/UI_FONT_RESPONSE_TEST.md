# Glyph completeness and local menu response candidate

This candidate addresses [#14](https://github.com/Calligula87/plus-vita/issues/14)
under [#27](https://github.com/Calligula87/plus-vita/issues/27). The accepted
blue/gold colours and retained-JSON sign-in flow/instructions stay unchanged.
The new UI timing support is a scoped part of
[#20](https://github.com/Calligula87/plus-vita/issues/20), not completion of the
whole release logging/privacy task.

## Software evidence and limits

The actual font adapter, text layout and full-screen painter now run in a host
regression harness with only external system-font calls replaced. A requested
36-pixel fixture glyph previously lost its upper part: 120 of 360 expected ink
pixels survived. Computing the baseline from requested-size glyph ink restores
all 360. This reproduces the failure pattern, not captured measurements of the
Vita font engine. Inspect actual glyphs on Vita before accepting the fix.

For a 96-character row, the former path made 13134 system-font calls cold and
13132 warm. Bounded glyph/line-metric caching reduces that fixture to 111 cold
and zero warm. Layout measurements drop from 4544 to 335 by measuring the final
line's overflow once. Welcome and catalogue focus changes exercise the same
painter as the native UI and reuse cached glyphs; cache-pressure checks exceed
the 256-entry / 512-KiB alpha budget without losing glyph output. These are
work-count results, not proof that the reported five-second Vita delay is gone.

The public [libvita2d PVF adapter](https://github.com/xerpi/libvita2d/blob/master/libvita2d/source/vita2d_pvf.c)
was consulted for the external raster/bearing conventions. No Sony font assets
are bundled. The physical font engine, presentation timing and perceived response
still require this hardware check.

## Vita check

1. Install the uniquely named candidate and record its filename/checksum from
   the handoff. Installing replaces the current app because the title ID is the
   same. Keep the previous VPK for rollback.
2. Open the app with the existing JSON in place. Confirm the accepted automatic
   sign-in still reaches the catalogue and the JSON stays unchanged. Use
   [the existing guide](ACCOUNT_SETUP.md) if another transfer is needed.
3. Inspect the catalogue heading, count/status, selected/unselected rows and
   footer. All glyph tops/bottoms should be complete. Check accented/non-ASCII
   text when available, long rows with explicit `...`, and normal viewing-size
   sharpness. Inspect home, About, setup, Account and a progress/error screen too.
4. Move the left stick once in each direction, press Cross/Circle, and touch
   buttons. Focus/action feedback should feel immediate. The engineering target
   is normally <=100 ms for an isolated local action after screen readiness;
   record an estimate as an estimate unless using a timed recording. There must
   be no multi-second freezes or delayed actions after release.
5. Hold the stick to browse for at least ten seconds; release it, reverse once,
   return home, enter About and reopen catalogue. Verify repeat/deadzone and
   focus remain predictable, and D-pad presses still do not navigate menus.
6. If comparing response, identify the earlier VPK actually being compared.
   The retained old bitmap candidate is `plus-vita-ui-feedback-1.vpk`, SHA256
   `441A8B83C185FC2EF9E623FB048599284526A920801205419DB5C66499E66B2B`;
   it is an available reference, not proof that it is the unnamed fast version
   in the maintainer's report. The prior failing system-font candidate was
   `plus-vita-retained-json-1.vpk`, SHA256
   `DA6A4F7EE0343548ACF5043CCC2D3743C5226C86B0203A0A2BF6E9D2D31B2404`.
7. Exit through the app's Exit action to drain final timing summaries. Copy
   `ux0:data/plus-vita-allocation.log` before relaunching: every graphical launch
   starts a fresh file. Look for `build=ui-font-response-1`, not an old streaming
   run. Missing identity or failed persistence means current evidence is unavailable.
8. Report clipping/response pass or fail separately, with screen/action details.
   Exact Vita response and final beta acceptance remain pending until tested.

## Interpreting the current UI log

The graphical app always uses the existing deferred sink, independent of the
historical diagnostic-stream feature option. Input/render sampling only updates
bounded numeric aggregates. At most 64 periodic snapshots and one final summary
are queued; the UI output stays bounded. Snapshots cover approximately the first
64 seconds, and normal Exit records whole-run totals/maxima. Closing the process
through the system may lose the final summary. Sink loss/I/O summaries must be
checked before drawing conclusions.

`draw` covers painting the back buffer; `present` covers frame submission/wait;
`sampled_input_to_present` covers a state-changing input sample through the next
submitted frame; `poll_gap` covers spacing between menu input polls. Keyboard
interaction time is excluded. These values omit the physical input's age before
sampling and are not full physical or cloud-game input response latency.

Only fixed stage/build fields and numeric durations/counts enter this UI path.
No game titles, displayed text, account labels, JSON, credentials or HTTP bodies
are passed into it. Never share account files or a whole data directory. The
remaining release-wide logger/settings/privacy requirements stay open in #20,
#21 and #22. Streaming media/control policies and accepted rollback artifacts
are unchanged; no Play or final 45-minute result is implied.
