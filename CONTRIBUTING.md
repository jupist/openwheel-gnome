# Contributing to openwheel

Thank you for contributing! This project aims to integrate well with KDE Plasma and the broader KDE ecosystem. Please follow the expectations below when preparing changes.

## Code of Conduct

All contributors are expected to follow the KDE Code of Conduct:
https://kde.org/code-of-conduct/

## GitLab merge request workflow

We use the standard KDE GitLab workflow:

1. Fork the repository on KDE GitLab (invent.kde.org).
2. Create a topic branch for your change.
3. Push the branch to your fork.
4. Open a Merge Request (MR) against the upstream project.
5. Respond to review feedback and keep the MR up to date.

## Target branch policy

* Target the default development branch (typically `master`/`main`) unless a maintainer asks for a backport.
* Backports should be proposed in separate MRs and must be clearly labeled.

## CI checks and expectations

* Keep CI green. MRs must pass all configured checks before they can be merged.
* If your change affects build or runtime behavior, include any required updates to tests or packaging metadata.

## Qt/KF requirements

This project targets the KDE Plasma 6 / KDE Frameworks 6 (KF6) stack:

* **Qt:** Qt 6 is required. Do not introduce new Qt 5 dependencies.
* **KDE Frameworks:** KF6 is required where KDE Frameworks APIs are used.
* Keep compatibility with the minimum Qt/KF versions used by Plasma 6 releases.

## Coding style expectations

* Follow existing formatting and naming conventions in each subproject.
* Prefer clear, self-documenting code and small, focused commits.
* Avoid unnecessary API/ABI changes unless agreed with maintainers.

## Licensing compatibility

* Contributions must be compatible with the project’s licensing (GPL/LGPL).
* Only include dependencies and code that are compatible with GPL/LGPL.
* When adding new files, include appropriate license headers consistent with existing sources.

## Plasma 6 / KF6 specifics

* Use Qt 6 and KF6 APIs; avoid deprecated Qt 5/KF5 interfaces.
* Ensure UI work aligns with Plasma 6 design conventions and platforms.
* If you add Plasma-specific integrations, note any version constraints in the MR description.
