# Supplied Plus Vita artwork

The maintainer supplied the square blue/gold logo and wide splash for #31–#34.
The original root-folder PNGs remain local. These normalized sources preserve
their RGB pixels while omitting the originals' non-image metadata.

Committed runtime resources are prepared offline, so the Vita performs no PNG
decoding, resizing or network loading during input/rendering:

- `icon0.png`: indexed 128×128 PNG, matching the VitaSDK sample icon format.
- `logo.pvi`: opaque 192×192 native RGBA welcome image.
- `splash.pvi`: opaque 960×544 native RGBA image, with the complete wide image
  fitted proportionally and centered on a matching dark background.

Each PVI has the eight-byte ASCII magic `PVIMAGE1`, little-endian 32-bit width
and height, then exactly width×height RGBA bytes. The loader accepts only the
specific expected dimensions and exact file length. Invalid images remain absent
independently, permitting the normal UI fallback.

Recreate derivatives from the normalized, committed sources with Pillow:

```sh
python tools/prepare-branding.py \
  --logo vita-client/assets/branding/logo-source.png \
  --splash vita-client/assets/branding/splash-source.png \
  --output vita-client/assets/branding
```

Builds consume the committed derivatives; Pillow is not a Vita/CI build
dependency. The package privacy gate verifies the actual shipped resources
against those sources and rejects unexpected image/archive metadata.
