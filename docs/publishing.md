# Publishing `flight`

`flight` is distributed without an AirLabs credential. Live-data users bring
their own API key and provide it through `AIRLABS_API_KEY` in their local
environment. Building or installing the executable does not capture the
maintainer's environment or copy its key.

## User workflow

After downloading or installing `flight`, each user should:

1. Create their own AirLabs account and obtain its unique API key.
2. Set `AIRLABS_API_KEY` in their shell environment or private shell startup
   file.
3. Run `flight BA281`, substituting the desired commercial designator.

Users must not be instructed to use a maintainer's personal key. Shared keys
couple every user's requests to one account, quota, and security boundary. If a
future hosted edition provides live data without requiring user keys, its
provider credential must remain on a controlled backend with authentication,
rate limiting, and caching; it must never be shipped to a client executable.
Provider terms and redistribution rights must be reviewed before offering such
a service.

Users without a key can run `make test` and all `--fixture` modes locally. These
paths do not contact AirLabs or OpenSky.

## Release checklist

Before publishing source, an archive, or a binary:

- Search the release contents and history for real credentials, raw provider
  responses, private `.env` files, and shell configuration files.
- Confirm `.env` and `.env.*` remain ignored and are absent from the artifact.
- Build from a clean tree with strict warnings and run `make test`.
- Exercise one fixture without any provider credentials.
- Confirm a missing key produces a clear authentication/setup result without
  displaying sensitive environment values.
- Confirm `--debug-provider` does not print credentials or raw provider JSON.
- Publish setup instructions that tell every user to obtain their own AirLabs
  key.
- Review the current AirLabs and OpenSky terms before commercial distribution
  or redistribution of provider data.

If any credential appears in a commit, build log, issue, chat, screenshot, or
release artifact, treat it as compromised: revoke or rotate it at the provider,
remove it from the affected artifact, and publish a corrected release. Removing
the visible text alone does not make an exposed credential safe again.
