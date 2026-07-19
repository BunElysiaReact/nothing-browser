# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest stable | ✅ |
| Older releases | ❌ |

We only patch security issues on the latest release. Please update before reporting.

## Reporting a Vulnerability

**Do not open a public GitHub issue for security vulnerabilities.**

Email us at: **ernestteschhouse@gmail.com**

Include:
- A description of the vulnerability
- Steps to reproduce it
- What you think the impact is
- Your system/version info

We'll acknowledge within **48 hours** and aim to ship a fix within **7 days** for critical issues.

## Scope

Things we care about:
- Remote code execution via the browser engine
- Data exfiltration through the network capture layer
- Fingerprint spoofing bypass that leaks real identity
- Arbitrary file read/write from the headless/headful daemon
- Dependency vulnerabilities in `newpipe-bridge`

Out of scope:
- Vulnerabilities in Qt6/Chromium themselves (report those upstream)
- Issues only reproducible on unsupported OS versions
- Self-inflicted issues from modifying the binary

## Disclosure Policy

We follow **coordinated disclosure**. Once a fix is released, you're welcome to publish your findings. We'll credit you in the release notes unless you prefer anonymity.