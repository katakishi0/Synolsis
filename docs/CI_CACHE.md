# GitHub Actions: faster Android builds with caches

**Status:** the main cached APK workflow is [`cache-build-test.yml`](../.github/workflows/cache-build-test.yml). It compiles our independent [`strato-full-core`](../../tree/strato-full-core) branch, with an optional `strato-fw23-zbic` input for testing that branch.

## What is cached

| Cache | Contents | Configuration |
|---|---|---|
| Gradle | Dependencies, transformed libraries, local Gradle build cache and Gradle wrapper distribution | `gradle/actions/setup-gradle@v4`; `cache-read-only: false` |
| Native C/C++ | Outputs reused by `ccache`, with a maximum internal size of **1 GiB** | `actions/cache@v4`, cache key includes OS, build configuration, source branch and run number; restores from most recent compatible entry |
| Android SDK, NDK, CMake | **Not cached**: use runner packages if present, install missing versions only | SDK 34, build-tools 34, NDK 26.1.10909125, CMake 3.22.1 |
| Experimental APK | Output for download, **not a dependency cache** | `actions/upload-artifact@v4` with `retention-days: 30` |

GitHub's dependency caches are immutable. The native cache key rotates on each run and restores the most recent matching entry, so newly compiled objects are saved rather than permanently locking an old snapshot.

### Set dependency-cache retention to 30 days

GitHub controls cache retention in **repository settings, not the workflow YAML**. Default is **7 days**.

1. Open [Settings → Actions → General](https://github.com/vps-generator/Skyline-Strato-Android/settings/actions).
2. Find **Cache settings**.
3. Set **Cache retention = 30 days**, then **Save**. If necessary, also review the **Cache size eviction limit**.
4. If the setting is unavailable, it can require an eligible plan or payment method and an administrator of `vps-generator`; the organization may also enforce a lower maximum.

**Important:** 30 days is the maximum *inactivity lifetime*, not a guarantee of storage. Entries can still be evicted sooner under the configured quota. This repository is private; GitHub supports up to 365 days for private/internal repos when eligible and organization policy allows it.

The `retention-days: 30` field in a build workflow controls **the ZIP/APK artifact only**, not the compilation cache. See [GitHub's official guide](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/enabling-features-for-your-repository/managing-github-actions-settings-for-a-repository#configuring-cache-settings-for-your-repository).

## Running cached builds

1. Open [Cached Strato Android APK Test](../../actions/workflows/cache-build-test.yml).
2. Choose **Run workflow**.
3. Select `strato-full-core` (the complete Skyline/Strato source in our GitHub) or `strato-fw23-zbic` if you are intentionally testing the experimental decoder.
4. Run once to populate Gradle/ccache. A subsequent run can be faster, depending on which sources change and cache hit rate.
5. Review the **Cache report** in the GitHub Actions job summary, then download the artifact ZIP.

There's also a manual [Cached Strato Android CI](../../actions/workflows/cached-android-ci.yml) for Python regression tests plus an optional standalone core build.

## Safety: old nested Oboe sample Gradle wrapper

Gradle's official validation identified one **unknown** wrapper JAR in the unused `app/libraries/oboe/apps/OboeTester` example. To avoid disabling validation for all wrappers, the cached workflows verify the exact SHA-256 of this unused file before excluding it **only from the CI worktree**. The Strato project's top-level Gradle wrapper and all other remaining wrappers are still validated by `setup-gradle`. The source stored in Git remains unchanged.

## Expected speedups and limits

The first build normally has a cold cache and is not accelerated. Warm builds can reuse Gradle downloads and C/C++ compilation outputs. Changes to the NDK, Gradle configuration, CMake flags or heavily modified source may reduce hits. GitHub runner disk/storage limits still apply.

Do not cache `prod.keys`, `title.keys`, game dumps, firmware or secrets. The workflows use only build dependencies and compiler outputs.
