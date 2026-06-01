# Security Policy

SiteIQ (repo: **SiteSync-AR**) is an iOS AR application for AEC professionals,
built on Unreal Engine 5.6, plus a static marketing site served from
`website/` via GitHub Pages. This policy explains what is in scope, how it is
maintained, and how to report a vulnerability.

## Supported Versions

This project is under active pre-release development. It has **no semantic
versioned releases, no App Store build, and no TestFlight distribution** —
device builds are wired Mac → iPhone only (Apple Personal Team). There is
therefore no back-version support matrix.

| What | Supported |
| --- | --- |
| `main` branch (latest commit) | ✅ Security fixes land here |
| GitHub Pages site (`website/`, deployed from `main`) | ✅ |
| Any older commit, tag, fork, or local build | ❌ Update to latest `main` |

Only the current `main` branch receives security fixes. There is no
long-term-support or patch branch — if you are on an older commit, pull the
latest `main`.

## Reporting a Vulnerability

**Please do not open a public GitHub issue for security problems.** Public
issues disclose the vulnerability before a fix is available.

Use one of these private channels instead:

1. **Preferred — GitHub Private Vulnerability Reporting.** Go to the
   [Security tab](https://github.com/jamesrusso12/SiteSync-AR/security) →
   **Report a vulnerability**. This opens a private advisory visible only to
   the maintainers and you.
2. If that is unavailable, contact the maintainer privately through their
   GitHub profile (**[@jamesrusso12](https://github.com/jamesrusso12)**) and
   ask for a secure channel before sending any details.

When reporting, please include as much of the following as you can:

- A description of the issue and its potential impact.
- Steps to reproduce, or a proof of concept.
- The affected area (iOS app, UE5/C++ source, MCP tooling, or the
  `website/` static site) and the commit hash you observed it on.
- Any suggested remediation.

### What to expect

This is a small two-person project (James and Cole), so responses are
best-effort rather than backed by a formal SLA:

| Stage | Target |
| --- | --- |
| Acknowledge your report | Within **7 days** |
| Initial assessment (accepted / needs info / declined) | Within **14 days** |
| Fix or mitigation for accepted issues | As soon as practical, prioritized by severity |

If a report is **accepted**, we will work on a fix, keep you updated through
the private advisory, and credit you when it ships (unless you ask to remain
anonymous). If a report is **declined** (e.g. out of scope, working as
intended, or not reproducible), we will explain why.

Please give us a reasonable opportunity to remediate before any public
disclosure.

## Scope

**In scope**

- The iOS app and its UE5 / C++ source under `SiteSyncAR/Source/`.
- The bundled `UnrealMCP` plugin and `dev/` tooling (note: the MCP TCP server
  binds `127.0.0.1:55557` and is intended for local developer use only).
- The static marketing site in `website/` and its GitHub Pages deployment,
  including the Content-Security-Policy, the frame-buster, and the Formspree
  waitlist form.
- This repository's GitHub Actions workflows under `.github/workflows/`.

**Out of scope**

- Third-party platforms and dependencies — Unreal Engine, ARKit/iOS, Formspree,
  Stripe, Supabase, and GitHub itself. Report those to their respective
  vendors.
- Vulnerabilities requiring a physical, jailbroken, or already-compromised
  device, or requiring the local MCP developer port to be deliberately exposed
  to a network.
- Social engineering, spam to the waitlist form, and denial-of-service
  reports against GitHub Pages.
- Findings in old commits or forks that are already fixed on `main`.

## A Note on Secrets

This is a public repository. Application secrets (Apple signing material,
Formspree/Stripe/Supabase keys, etc.) must never be committed. If you find a
secret, credential, or private key checked into the repository or its history,
treat it as a vulnerability and report it through the private channel above so
it can be rotated.
