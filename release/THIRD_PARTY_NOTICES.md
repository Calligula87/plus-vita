# Source, credits and licenses

Plus-Vita's native application is distributed under the GNU Affero General
Public License version 3. See [LICENSE](../LICENSE). Third-party code, fonts and
artwork retain their respective notices and terms. The original upstream
copyright and SPDX notices remain in the source files.

The v000 executable corresponds to commit
`ae350183b44b663b76f7683933597fcbfdd514a7`. Its dependencies and toolchain identity
are recorded in [v000.json](v000.json). The release provides the VPK and a matching
source snapshot separately; users do not need source code to run the app.
GitHub's automatic source archives omit submodule contents. Use the provided
`plus-vita-v000-source.zip`, or clone the repository with its pinned submodules:

```sh
git clone --recurse-submodules https://github.com/Calligula87/plus-vita.git
cd plus-vita
git checkout ae350183b44b663b76f7683933597fcbfdd514a7
git submodule update --init --recursive
```

| Component | Notice / license |
| --- | --- |
| Pylux and its libchiaki implementation | [AGPL-3.0 with upstream OpenSSL additional permission](licenses/pylux.txt) |
| json-c | [MIT-style license](licenses/json-c.txt) |
| nanopb | [zlib license](licenses/nanopb.txt) |
| Opus | [BSD license and additional notices](licenses/opus.txt) |
| GF-Complete | [BSD license](licenses/gf-complete.txt) |
| Jerasure | [BSD license](licenses/jerasure.txt) |
| stb_image | [MIT alternative](licenses/stb-image.txt) |
| FreeType | [FreeType License](licenses/freetype.txt), [third-party notices](licenses/freetype-third-party.txt) |
| Noto Sans | [SIL Open Font License 1.1](licenses/noto-sans.txt), [font notice](licenses/font-notice.txt) |
| libcurl 8.21.0, supplied by the tested SDK | [curl license](licenses/curl.txt) |
| Mbed TLS 3.6.5, supplied by the tested SDK | [Upstream dual-license text](licenses/mbedtls.txt); Apache-2.0 option used here |
| zlib 1.3.2, supplied by the tested SDK | [zlib license](licenses/zlib.txt) |
| Zstandard 1.5.7, supplied by the tested SDK | [BSD license](licenses/zstd.txt) |

The license files are also distributed together as
`plus-vita-v000-licenses.zip` beside the VPK. The VPK's embedded `licenses/`
directory contains the original packaging subset; the supplementary archive
provides the remaining upstream notices without changing the tested VPK.

Vita support uses the open-source [VitaSDK](https://vitasdk.org/). SDK and target
runtime sources are maintained by [VitaSDK](https://github.com/vitasdk), including
[newlib](https://github.com/vitasdk/newlib),
[pthread-embedded](https://github.com/vitasdk/pthread-embedded), and
[package recipes](https://github.com/vitasdk/packages). Their own licenses apply.
Unmodified SDK-supplied curl, Mbed TLS, zlib and Zstandard sources are available
from [curl 8.21.0](https://github.com/curl/curl/tree/curl-8_21_0),
[Mbed TLS 3.6.5](https://github.com/Mbed-TLS/mbedtls/tree/mbedtls-3.6.5),
[zlib 1.3.2](https://github.com/madler/zlib/tree/v1.3.2), and
[Zstandard 1.5.7](https://github.com/facebook/zstd/tree/v1.5.7).

Custom artwork was supplied by the maintainer, with image-generated background
and wordmark derivatives. [Artwork preparation and provenance](../vita-client/assets/branding/README.md)
are documented. No user account file, private device photo, game artwork or Sony
font file is included as a custom branding asset.
