# Development control guide

These controls describe current source implementing #37, #38 and #39. The
downloadable v000 remains the accepted stream build and retains its original
fixed mappings and stick/touch menus. The new behavior is a candidate until the
[hardware checks](FEEDBACK_CONTROLS_TEST.md) pass.

## Menus

Use the D-pad or left stick, Cross to select and Circle to go back. Holding a
direction repeats after 400 ms and then every 160 ms. In the catalogue, left/right
also move through games. D-pad input takes precedence over the stick, and opposing
D-pad directions cancel. The two input methods cannot double-step focus.

A touched button shows its pressed state immediately. Lift inside the original
button to select it. Sliding outside cancels the entire gesture, even if you slide
back before lifting. Additional contacts cancel the gesture. Screen changes use a
120 ms fade; input during that transition is consumed and cannot select the next
screen's controls. These rules affect app menus, not gameplay touch.

## One saved gameplay layout

Open **Settings and help → Control mapping → Edit input bindings**. Choose an
input, then its game action or **Disabled**. Previous/Next shows the remaining
inputs or actions. Physical L2/R2/L3/R3 button rows are labelled **PS TV only**
on Vita. The separate **Rear touch top-left/top-right/bottom-left/bottom-right**
rows are the Vita touch zones, defaulting to L2/R2/L3/R3 respectively. Hardware
availability labels are separate from the **Disabled** game-action choice;
every binding remains editable for the same global layout.

- Each input performs one action or none. Several inputs can share an action.
  A shared action stays active until all contributors release it.
- Buttons and rear-touch quadrants mapped to L2/R2 produce full pressure.
  Analog triggers mapped to L2/R2 retain pressure; shared trigger contributions
  use the greatest pressure. An analog trigger mapped to a digital action is
  pressed at values 128–255 and released at 0–127.
- **Front-touch movement** independently controls touch contacts/coordinates.
  The two-finger input has its own binding, including Disabled. Turning movement
  off does not automatically disable that binding.
- **Save for next session** persists the complete layout. **Cancel** or Circle
  discards changes. **Restore defaults** changes the draft; Save is still required.
  A failed save leaves the previous saved layout intact and reports the failure.
- The saved layout is copied when Play starts and remains fixed for that session.
  There is no editor during live play. Layouts survive app restarts.
- Menu controls, stick axes and the physical exit shortcut stay fixed. The
  system PS button remains system-owned. There are no per-game/controller
  profiles, macros, turbo or stick tuning.

The layout is stored separately from sign-in data at
`ux0:data/plus-vita-controls.dat`. A missing or invalid layout uses defaults;
invalid records are never partially applied.

## Vita defaults

Physical game buttons and both sticks retain their accepted assignments.
L/R are L1/R1; START is Options and SELECT is Share. Rear top-left/top-right are
L2/R2, rear bottom-left/bottom-right are L3/R3. Front contacts move the game
touchpad; two contacts press it.

Hold physical **START + SELECT for two seconds** to stop. Remapping or disabling
their individual game actions cannot disable this gesture. While held together,
their mapped game outputs are suppressed.

## PS TV candidate support

Pair a **DualShock 3 or DualShock 4** in PS TV system settings: connect by USB,
then press PS. Pairing and controller drivers belong to the operating system.
Use one active supported controller. Physical face buttons, D-pad, both sticks,
L1/R1, L2/R2 and L3/R3 feed the same saved layout.

DualShock 3 START/SELECT correspond to Options/Share. DualShock 4 uses its
OPTIONS/SHARE labels. Hold that pair for two seconds to stop, regardless of
gameplay mappings. Assign an ordinary button to **Touchpad click** when a game
requires it. Native DualShock 4 touchpad movement, motion and rumble are outside
this version. PS2-controller adapters are not covered.

On controller disconnection, all game inputs release and the stream stays
running while the cloud session remains healthy. Reconnect and release buttons,
triggers and sticks to neutral before gameplay input resumes. A second pad does
not replace a still-connected active pad. A cloud/network failure still follows
the existing session recovery behavior.

Real PS TV tests with each controller type are required before claiming hardware
compatibility. See [Sony's controller instructions](https://manuals.playstation.net/document/en/pstv/basic/usecontroller.html).
