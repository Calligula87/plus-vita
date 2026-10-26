# Supplied Plus Vita artwork

The maintainer supplied the square blue/gold logo and wide splash for #31–#34.
The original root-folder PNGs remain local. These normalized sources preserve
their RGB pixels while omitting the originals' non-image metadata.

Committed runtime resources are prepared offline, so the Vita performs no PNG
decoding, resizing or network loading during input/rendering:

- `icon0.png`: indexed 128×128 PNG, matching the VitaSDK sample icon format.
- `menu-source.png`: logo/text-free background plate edited with built-in
  imagegen from the supplied wide artwork. No private device photo was used.
- `menu.pvi`: opaque 960×544 background for every menu, prepared at 45% source
  brightness for readable white/gold text. It is loaded once; no hot-path file
  access, image decoding or blending is required. The welcome logo is removed.
- `splash.pvi`: opaque 960×544 native RGBA image, with the complete wide image
  fitted proportionally and centered on a matching dark background.
- `livearea/bg.png` and `startup.png`: indexed 128-color, metadata-free
  840×500 and 280×158 derivatives of the original branded wide artwork.
  `template.xml` installs these for the Vita LiveArea Start/Continue page.
  Layout and image constraints follow the
  [VitaSDK samples](https://github.com/vitasdk/samples).

Each PVI has the eight-byte ASCII magic `PVIMAGE1`, little-endian 32-bit width
and height, then exactly width×height RGBA bytes. The loader accepts only the
specific expected dimensions and exact file length. Invalid images remain absent
independently, permitting the normal UI fallback.

Recreate derivatives from the normalized, committed sources with Pillow:

```sh
python tools/prepare-branding.py \
  --logo vita-client/assets/branding/logo-source.png \
  --splash vita-client/assets/branding/splash-source.png \
  --menu vita-client/assets/branding/menu-source.png \
  --output vita-client/assets/branding
```

Builds consume the committed derivatives; Pillow is not a Vita/CI build
dependency. The package privacy gate verifies the actual shipped resources
against those sources and rejects unexpected image/archive metadata.

The built-in imagegen edit prompt for `menu-source.png` was:

> Use case: precise-object-edit. Edit target: the supplied wide Plus Vita artwork.
> Create a clean matching background plate for the app menus. Remove the ENTIRE
> central rounded-square icon, gold cross, blue V, PLUS-VITA lettering, and their
> gold/logo-shaped reflections. Seamlessly reconstruct only the blue sci-fi
> environment behind them. Preserve the original deep navy/electric blue palette,
> cyan crossing light trails, small luminous nodes, subtle radial beams and glossy
> floor reflections, same wide composition. The center must be an open continuous
> background with blue light and floor, no replacement object or icon. Absolutely
> no text, letters, symbols, logo, badge, watermark or UI elements anywhere. Keep
> the left/central areas fairly quiet and dark for readable menu text. Landscape
> 16:9. This is a background image only, not a screenshot or a UI mockup.
