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
Installation and the initial read-only inventory are complete. Findings are in
[`installed-client-audit.md`](installed-client-audit.md). The next step is one
user-authorized, redacted runtime observation of normal login and a short
streaming session.

No credentials, tokens, cookies, account profiles, or authenticated captures
will be collected or committed.
