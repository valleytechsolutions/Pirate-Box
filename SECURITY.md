# Security

Pirate Box currently publishes its C5 source checkpoint. There is no Pirate Box binary release or hosted installer yet. For local builds, verify the build receipt and use only images for the exact ESP32-C5-LCD-2.73 hardware profile.

## Reporting

Use the repository’s **Security → Report a vulnerability** option when available. Otherwise, open an issue with a brief impact summary and request a private contact channel before sharing exploit details, credentials, or captures.

## Data handling

- Wi-Fi profiles are stored in the device’s unencrypted NVS. Anyone with physical access may be able to recover them. Remove saved profiles before transferring a device to someone else.
- Captures, encounter history, badges, and profiles can contain private data. Review them before sharing logs or screenshots.
- Local serial tools require explicit board/port selection. The C5 flash wrapper checks a backup and writes four firmware segments, excluding NVS and existing filesystem regions. Read the board guide before use.
- No Pirate Box web flasher or visit counter is deployed by this repository.

## Repository checks

Release packaging rejects private workstation paths in firmware images. Private working files, local environments, backups, and captures are excluded from source control. Secret scanning complements code review; a passing scan does not establish the absence of every vulnerability.
