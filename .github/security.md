# Security Policy

## Supported Versions

This is a research and preservation project. There are no "released versions" in the traditional sense. The `main` branch is the current focus of development.

## Reporting a Vulnerability

If you discover a security vulnerability **in the tooling itself** (e.g., a buffer overflow in the proxy DLL, unsafe deserialization in a Python script, or a supply-chain issue in a dependency), please report it responsibly.

**Do NOT** open a public issue for security-sensitive bugs.

Instead, contact the maintainer directly:

- GitHub: [@e-gleba](https://github.com/e-gleba)
- Email: See GitHub profile for contact options (if available)

Please include:
- A clear description of the vulnerability
- Steps to reproduce (if safe to share)
- Potential impact assessment
- Suggested mitigation (if known)

We will acknowledge receipt within 7 days and aim to provide a fix or public disclosure timeline within 30 days.

## Scope

Security reports should relate to **this repository's code**. We do not accept reports about:
- The original AirStrike 3D game binaries (these are third-party copyrighted software)
- ASProtect packing techniques (known, historical)
- BASS audio library vulnerabilities (report to [un4seen](https://www.un4seen.com/))

## Legal Note

This project involves reverse engineering and runtime code injection (DLL proxy). These techniques are used solely for educational research and game preservation. Use responsibly and in compliance with your local laws.
