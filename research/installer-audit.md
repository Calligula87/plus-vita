# PlayStation Plus PC installer audit

## Artifact

- Filename: `PlayStationPlus-12.7.0.exe`
- Product version: `12.7.0`
- Size: `115,672,832` bytes
- SHA-256:
  `93DABD7A537A0B67A6732DCAF0F868199EB0E426C92F6D929FB1C93D32DCF0C1`
- Authenticode status: valid
- Signer: Sony Interactive Entertainment LLC
- Certificate issuer: DigiCert Trusted G4 Code Signing RSA4096 SHA384 2021 CA1

The installer itself is deliberately excluded from this repository.

## Initial result

The file is a signed Windows PE installer and does not expose its payload as a
normal archive through the built-in archive tools used during the first audit.
The next step is installation using the default location, without signing into
PSN, followed by an inventory of installed executables, libraries, drivers,
resources, configuration, media dependencies, and service endpoints.

No credentials, tokens, cookies, account profiles, or authenticated captures
will be collected or committed.

