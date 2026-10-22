# Artwork decoder

`stb_image.h` is stb_image 2.30 from the official nothings/stb repository,
commit `013ac3beddff3dbffafd5177e7972067cd2b5083`.

Source: https://github.com/nothings/stb/blob/013ac3beddff3dbffafd5177e7972067cd2b5083/stb_image.h

SHA256: `594C2FE35D49488B4382DBFAEC8F98366DEFCA819D916AC95BECF3E75F4200B3`.
The MIT alternative is reproduced in `stb-image-license.txt` and packaged with
the graphical app. No artwork is bundled.

The adapter enables only PNG/JPEG, limits downloaded input to 2 MiB and image
dimensions to 1024 per axis, and enforces an 8 MiB decoder allocation budget.
One background worker decodes into a six-entry, 128-square RGBA memory cache.
