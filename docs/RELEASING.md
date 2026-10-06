# Releasing Coda

Open **Actions → Release → Run workflow**, select **main**, and enter a stable
version such as `3.0.1` or `v3.0.1`. The workflow:

1. Validates the version and rejects existing tags or older versions.
2. Updates `pyproject.toml` and runs all five binding test suites.
3. Pushes the version commit and annotated `v` tag atomically to `main`.
4. Builds the platform wheels and native libraries from that exact commit.
5. Publishes to PyPI and creates a GitHub release with generated notes and assets.

You do not need to edit the version, commit, tag, or push locally. If the version
was already updated on `main`, the workflow creates only the tag.

The existing `release` environment and PyPI trusted publisher configuration are
used. Repository rules must allow the workflow's built-in `GITHUB_TOKEN` to push
to `main` and create release tags. The workflow does not bypass branch protection:
if a rule rejects either update, the atomic push leaves both refs unchanged.

If building or publishing fails after the push, use **Re-run failed jobs** on
that run. Starting a new run with the same version or re-running all jobs will
reject the existing tag. If PyPI publishing succeeded but GitHub release creation
failed, create the GitHub release from the existing tag and attach the saved run
artifacts; publishing that version to PyPI again is not supported.

Manual tag pushes still work, but the tag version must match `pyproject.toml`.
Releases are serialized so two runs cannot publish concurrently.
