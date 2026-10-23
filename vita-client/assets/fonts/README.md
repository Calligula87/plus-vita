# Comparison font assets

The optional `PLUS_VITA_FONT_COMPARISON` profile bundles unmodified Noto Sans
Regular from the official `notofonts/noto-fonts` repository, commit
`ffebf8c1ee449e544955a7e813c54f9b73848eac`, path
`hinted/ttf/NotoSans/NotoSans-Regular.ttf`.

SHA256: `B85C38ECEA8A7CFB39C24E395A4007474FA5A4FC864F6EE33309EB4948D232D5`.
Copyright and SIL Open Font License 1.1 are reproduced in `OFL.txt`.

The FreeType submodule is pinned to 2.14.3, commit
`0a0221a1347e2f1e07c395263540026e9a0aa7c7`. The selected FreeType License and
additional component notices are packaged with `NOTICE.txt`.

The alternative adapter reads at most 1 MiB of font data into memory, limits
FreeType allocations to 4 MiB and caches at most 256 glyphs/512 KiB of alpha.
Rasterization uses exact pixel sizes and grayscale output. Missing Noto glyphs
use the existing runtime system-font fallback; no Sony font file is distributed.
The default graphical and historical diagnostic builds do not bundle this font.
