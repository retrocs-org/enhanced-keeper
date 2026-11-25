# Keeper Enhanced website changelog

Changes to the PHP website and HTTP API. Game and dedicated-server changes are
recorded separately in the repository's root `CHANGELOG.md`.

## 0.1.0 — Initial website

### Website

- Add a public credits page acknowledging KeeperFX, FXGraphics and FXsounds,
  with pinned upstream license links and repository credits.

- Introduce a standalone PHP website for Keeper Enhanced, a fork of KeeperFX.
- Add Overview, Features, Network, Servers, Leaderboard, Downloads and account pages.
- Group server listings, events and replays under Network with subpage navigation.
- Add a simplified Dungeon Keeper, KeeperFX and KeeperFX Enhanced feature timeline.
- Link the original Dungeon Keeper Gold game to its Steam store page.
- Use dungeon-inspired olive, gold and ember colors with a simpler page layout.
- Remove the decorative lair background and keep the API browser out of navigation.
- Show account-dependent controls according to sign-in state.
- Show the public GitHub repository as the project's source link.

### Services and API

- Keep the PHP website and HTTP API together inside `public-web/`.
- Integrate account, statistics, public server-directory, event and replay pages.
- Support MariaDB configuration through `config.inc.php`.
- Include command-line database checks and schema migration tools.
- Retain LAN and direct-address play independently of website accounts.

### Deployment fixes

- Provide Apache routing guidance for application URLs behind HAProxy.
- Preserve bearer authentication and trusted forwarded HTTPS/client information.
- Protect application internals and configuration from direct HTTP access.
- Preserve the deployed `config.inc.php` when refreshing the website.
- Include a configuration sample and document remote MariaDB setup.

These notes describe the initial website work; they do not imply that every
service is enabled or that a release has been published on a particular server.
