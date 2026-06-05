---
title: Versioning
layout: default
parent: Development
nav_order: 1
---

# Versioning

Releases use SemVer tags: `vMAJOR.MINOR.PATCH`.

The component manifest version uses plain SemVer. Product firmware should pin this repository with exact tags, not branches.

For a release-intended change:

- Update the Kconfig release identity defaults.
- Update the component manifest version and dependency pins.
- Build `examples/generic_device`.
- Commit the release changes.
- Create an annotated tag only when a release is explicitly requested.

Product configs should expose the same version identity through the Basic cluster by setting `date_code`, `sw_build_id`, `application_version`, and `hardware_version`.
