# Security Policy

## Supported Versions

| Version | Supported |
|---|---|
| 3.x.x | ✅ Current |
| 2.x.x | ⚠️ Critical fixes only |
| 1.x.x | ❌ End of life |

## Reporting a Vulnerability

If you discover a security vulnerability, please report it responsibly:

1. **Do NOT** open a public issue
2. Email the maintainer directly or use [GitHub Security Advisories](https://github.com/xAstroBoy/quest-ue4-modloader/security/advisories/new)
3. Include:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if any)

## Scope

This modloader runs with root privileges on Quest devices. Security considerations include:

- **Code injection**: The modloader injects code into running processes. This is by design.
- **Network bridge**: The ADB bridge (port 19420) accepts commands from localhost only.
- **File access**: Mods have full filesystem access on the device.
- **No telemetry**: The modloader does not collect or transmit any data.

## Out of Scope

- Vulnerabilities in the game itself (report to the game developer)
- Issues requiring physical device access (device is already rooted)
- Social engineering attacks
