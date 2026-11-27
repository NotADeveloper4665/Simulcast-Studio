# Upstream integration plan for Simulcast Studio

Review date: 2026-11-27. This is a plan, not an upstream merge or a claim of runtime compatibility. No application code changes are included in this document's PR.

## Verified baseline and scope

- Simulcast master: `f1aabe69c274cc17762427a183520e8479721edd`.
- Upstream obs-rust/obs-studio master: `723cd1c803bdee2e57f782a969203de768d45677`.
- Common ancestor: `fa6e0bebb4e74b721d79de85448f0c3cbdfa8759`.
- [Pinned comparison](https://github.com/obs-rust/obs-studio/compare/NotADeveloper4665:f1aabe69c274cc17762427a183520e8479721edd...723cd1c803bdee2e57f782a969203de768d45677): 40 upstream-only commits, 30 Simulcast-only commits. These are ancestry counts, not 40 missing user-facing features.
- Compatibility branch: `6ca9a8ff7397ad586cc4ea31539624c797388143`; macOS branch: `58243606444add23d55e2beab86b4fa88fddb377`; existing compiler-repair PR [#3](https://github.com/NotADeveloper4665/Simulcast-Studio/pull/3): `f397d3e80afba2afaa382f67ce5c2e0da6f80a8c`.
- Reviewed the 40 commit records and corresponding merged PR descriptions/file lists, all 74 open PR titles/descriptions from the REST collection, and detailed file lists for 28 relevant open PRs. Read selected implementation patches and the local cloning, portrait Program, clean-source recording and master-gain call sites. This is not a line-by-line audit of every patch or every review thread.
- The aggregate GitHub comparison is limited to 300 files. Combining the 40 individual commit file lists gives **315 unique paths**. None intersects the paths changed since the common ancestor by Simulcast master, compatibility or macOS branches. That reduces direct edit conflicts; it does not establish runtime safety or prove that a merge is conflict-free. Shared build fragments and external action dependencies still need validation.
- Upstream [master push run 45962407898](https://github.com/obs-rust/obs-studio/actions/runs/45962407898) succeeded for the pinned head. This is upstream evidence only. Simulcast PR #3's [final-head run 45956743027](https://github.com/NotADeveloper4665/Simulcast-Studio/actions/runs/45956743027) failed; its release-target cross-lint passed, while other lanes remain red. Do not assume that PR or the fork baseline is fully validated.

No upstream change in this merged range implements Simulcast's independent portrait Studio Mode, multistream controls, shared-capture clone UI, CPU/GPU background removal, branded macOS browser package or signed virtual-camera distribution. Keep those systems and their branch availability explicit.

## Integration approach

Use small integration branches from current Simulcast master. Preserve upstream authorship and provenance (for example `git cherry-pick -x` on the selected squash commits), including each port's native adapters, export/hidden-symbol fragments, original-C oracle, registration metadata and tests. Importing just the Rust implementation would leave incomplete ABI/build/test registration.

The batches below are review boundaries. Within each batch preserve upstream chronological order and carry prerequisites before consumers. Recheck dependencies against the resulting tree: compiler success alone does not prove that an API port retained scene and device behavior. Do not copy whole upstream files over Simulcast files or replace Simulcast CI/package identities.

After each batch, run GitHub Actions on the exact integration head, record results and retain a revertable checkpoint. Finish the existing compiler/lint baseline work without redoing its fixes; recheck PR #3 and take each fix only once. Keep a list of imported and intentionally deferred upstream SHAs. A selective import must never be described as fully caught up.

Do not merge the experimental branches into alpha merely to apply upstream fixes. Apply a verified shared-base change to each experimental branch through a separate reviewed update; its branch-specific tests and packaging must pass again. The macOS and compatibility branches remain distinct.

## Ordered batches and decisions

### A. Restore dependable CI before changing runtime behavior — 7 commits

Take the portable parts of #5615 (Tier 2 scratch cleanup), #5747 (runner placement reporting), #5764 (explicit cache-daemon restart), #5778 (stock apt closure recovery), #5780 (Windows Tier 2 timeout), #5798 and #5808 (AI-rebase read-failure handling), with their tests.

These target existing inherited tooling and do not replace the Simulcast Fedora/macOS workflows. Check action pins, repository guards and the cache-daemon CLI contract; preserve bounded retries and signed apt indexes. The AI-rebase changes only affect tooling when enabled and must not enable automated rebasing or merging on this fork.

Acceptance: CI self-tests and actual hosted Tier 2 setup succeed; cache failures retain the uncached fallback; all required checks remain required. Do not import upstream infrastructure identifiers just because they appear in the same workflow.

### B. Bring over the foundational source/frame ports — 10 commits

Take #5657 defaults, #5637 frame initialization, #5739 settings update, #5746 draw/color-matrix, #5741 frame copy, #5752 CEA708 dispatch, #5761 frame-entry wrappers, #5765 preloaded-video consumer, #5767 async timestamp getter and #5758 frame get/release.

These replace inherited C orchestration through the existing OBS C ABI. They complement the two background-removal implementations; they do not replace either inference pipeline. Preserve callback/signal ordering, frame references, caller-owned buffers, alpha/color-range metadata and existing NULL behavior. #5761 precedes its preload consumer #5765; keep all frame ownership adapters and their static test link support.

Acceptance: original-C/hybrid differential tests, ABI/layout/export checks, async capture and deinterlace tests; real webcam hide/show, reconnect and resizing on macOS/Linux; browser transparency and audio; CPU/GPU filters under simultaneous previews, portrait Program, streaming and recording. Check for stale masks, extra capture instances and added frame queues. No claim of lower latency without measurement.

### C. Integrate lifecycle APIs underneath Simulcast's existing controls — 7 commits

Take #5776 filter ordering, #5789 scene-item ordering, #5794 filter copy, #5796 filter removal, #5800 filter addition, #5804 source removal and #5806 source duplication in upstream order, preserving their tests and hidden adapters.

Important integration boundaries:

| Upstream work | Existing Simulcast owner | Required treatment |
|---|---|---|
| #5806 source duplication | `OBSBasic::CloneSourceWithFilters` | Keep the group wrapper around the original source. The inspected code uses `obs_scene_add(scene, source)` then `obs_scene_insert_group`; replacing this with ordinary `obs_source_duplicate` can create another device/source instance. Adopt the upstream API port for its existing callers. |
| #5794 / #5796 / #5800 / #5776 filters | clone group filters; CPU/GPU filter modules | Preserve independent group filters and shared original-source properties. Verify callback/signal order, removal during rendering and reference ownership. Never merge the CPU and GPU algorithms. |
| #5789 scene-item ordering | portrait source order and native scene tools | Preserve reorder signals, group behavior, persistence and private Program snapshots. Do not rebuild the portrait editor. |
| #5804 source removal | portrait scenes, private Program and clean-source canvas | Check public/private signals, retain/release ordering and removal while outputs are active. Shared captures must survive while referenced elsewhere. |
| #5806 / #5794 private copying | `OBSBasic::CommitVerticalProgram` | Keep `OBS_SCENE_DUP_PRIVATE_REFS`: a private layout snapshot shares source references, so property/filter edits can still affect both views. Do not convert it into independent captures or overwrite stored Preview/Program UUIDs. |

Acceptance: reload existing profiles/collections; duplicate/reorder/delete scenes and groups; edit Preview without changing Program layout; verify no duplicate webcam activation; independent clone filters; remove shared sources during dual output; standard frontend/plugin API behavior and hotkeys. Use fixture device-open counts and source identity where possible, then actual hardware.

### D. Add profiler and audio scheduling as one dependency-aware series — 4 commits

#5774 adds the audio-thread profiler root. #5781 adds Linux real-time scheduling through direct permission, Realtime portal or rtkit, with high/default-priority fallback. #5783 builds on that platform split and startup hook to add macOS time-constraint policy. **Always include #5791 with #5781**: it replaces the glibc-2.30 `gettid` wrapper with `SYS_gettid`, addressing an AppImage link failure on the older glibc target.

The scheduler changes are in `rust/obs-media-io/src/ffi/audio_output.rs`; Simulcast's master gain is in `rust/obs-core/src/ffi/audio_callback.rs::audio_callback`. The inspected scheduler patches leave the existing mixing callback in place. Preserve `obs_set_master_volume_percent` and the once-per-mixed-track gain application; do not move it into an individual destination or UI-only control.

Retain the Linux `RLIMIT_RTTIME`/SIGXCPU demotion guard and permission-denied fallback; no root requirement or security workaround. The macOS result reported upstream is not a Simulcast benchmark.

Acceptance: GitHub Actions builds with and without GIO and all package architectures; verify minimum-glibc link; deterministic gain checks at 0/50/100% across multiple output tracks and recording; all simultaneous outputs; denied scheduling/portal absence; restart and shutdown; stall/demotion recovery. Measure audio gaps and A/V sync under load on target hardware, including browser and background removal.

### E. Keep infrastructure and optional packaging changes conditional — 12 commits

| PRs | Decision |
|---|---|
| #5552 pool canary | Defer: monitors upstream's fleet signal. It adds a workflow; do not introduce it without the repository-required explicit sign-off and an actual fork need. |
| #5607 fleet cache, #5743 routing, #5748 ready leases, #5749 checkpoints | Defer deployment until this fork has verified access/configuration for that infrastructure. Preserve hosted fallback. Do not assume obs-rust org membership, OIDC access, URLs or runner labels. Compatible action-pin maintenance can be reviewed independently. |
| #5745 attested cross-link deferral, #5754 queue lint skips, #5755 link deduplication | Review as one workflow/gate-manifest contract after baseline recovery. Keep real checks for this fork when trusted admission does not apply. Never copy skip predicates without proving equivalent coverage on the exact merge result. |
| #5753 CPU-pair batching, #5759 local handoff | Optional optimization after correctness. Keep the complete ordinary CTest/artifact path when reserved CPUs or a compatible local handoff are unavailable; establish prerequisites and run the harness tests first. |
| #5782 cache migration notes | Reuse relevant guidance; Windows remains on its supported cache path. Adapt wording to this fork's actual workflows. |
| #5784 Flatpak zccache | Defer to a dedicated Flatpak packaging task. Does not accelerate Fedora RPM/AppImage or macOS DMG by itself. If enabled, preserve app identity, checksum pins, saved-profile runtime checks and unshipped build tools; verify actual SDK execution and cache reuse. |

## Open upstream PR watchlist

At review there are 74 open PRs. Open work is not part of the 40-commit merged backlog; status and SHAs must be rechecked before import.

| PR(s) | Priority and integration decision |
|---|---|
| [#5810](https://github.com/obs-rust/obs-studio/pull/5810), head `26f09d1b4760` | High-priority mixer shutdown fix: `VolumeControl` owns its source UUID and ignores queued activation changes after destruction. Upstream CI Gate and Qt check succeeded at review, but it was still open. Prefer its merged commit; a deliberate backport must carry its test. Preserve Simulcast's separate master slider in `AudioMixer.cpp.hpp`. |
| [#5802](https://github.com/obs-rust/obs-studio/pull/5802), head `5f9c735d8f38` | Filter restore follows batch C; CI still running at review. Preserve the documented existing hybrid private-filter loading rule and saved clone filters; do not silently change private loads to public ones. |
| [#5627](https://github.com/obs-rust/obs-studio/pull/5627), head `b6f6f261295a` | Encoder weak-reference port; upstream CI Gate succeeded, still open. Candidate after merge, with multistream/recording stop and restart coverage. No reason to replace Simulcast output ownership. |
| [#5723](https://github.com/obs-rust/obs-studio/pull/5723), [#5709](https://github.com/obs-rust/obs-studio/pull/5709), [#5547](https://github.com/obs-rust/obs-studio/pull/5547), #5644/#5645 | Canvas reset, removal, channel setter and scene lookup. Highest Simulcast integration risk: horizontal/vertical/private clean canvases and active-output admission. #5723's gate failed a Windows Rust test at review; defer until resolved, then test both canvases independently. |
| [#5735](https://github.com/obs-rust/obs-studio/pull/5735), #5730/#5732, #5727, #5728 | Active-tree enumeration, settings reset, dimensions and color space. Audit master-gain input enumeration, clone visibility, clean-record dimensions and both mask pipelines. Draft work waits for complete implementation and validation. |
| [#5725](https://github.com/obs-rust/obs-studio/pull/5725) | Delayed-output reconnect ordering fix. Candidate after upstream validation; test multiple independent destinations, preserve-delay reconnect, aggregate controls and normal Start/Stop behavior. |
| #5630/#5673, #5674, #5696/#5701, #5706/#5708, #5736/#5737/#5738 | Transform, reorder, visibility and scene-prune ports/extractions/tests. Reuse Simulcast's native portrait tools and private Program; distinguish characterization/extraction from completed ports. |
| #5652/#5654, #5656/#5661, #5658/#5660, #5667/#5668, #5669/#5670, #5682-related #5683/#5684, #5688/#5689, #5690/#5691, #5692/#5693, #5698/#5699, #5700/#5702, #5716, #5679, #5632 | Encoder/output/service changes: protect destination orientation, encoder/audio track assignment, preferred sizes, regular recording/frontend handle semantics, paired/clean recording and references. Do not import every extraction/test pair as if it were a new feature. |
| [#5713](https://github.com/obs-rust/obs-studio/pull/5713), #5646/#5647, #5643/#5649 | Plugin loader, disabled modules and data lookup. Preserve compatibility-branch search layouts, environment paths, exact OBS ABI and CEF data discovery. Do not replace the frontend plugin loader with a separate Rust plugin framework. |
| #5711/#5707, #5662/#5663, #5675/#5676, #5680/#5681 | Rendering callback, transition UUID, hotkey-pair and caption extraction/characterization. Adopt completed work once merged and verify callback lifetime/ownership with both canvases and existing hotkeys. |
| [#5554](https://github.com/obs-rust/obs-studio/pull/5554), [#5555](https://github.com/obs-rust/obs-studio/pull/5555) | Optional VST3 editor/packaging series, with prerequisites outside the backlog. Separate task; do not enable VST3 by default or disturb existing VST2. Native Windows/macOS runtime verification is still needed in descriptions. |
| [#5155](https://github.com/obs-rust/obs-studio/pull/5155) | Texture-file orchestration has an explicit native macOS runtime hold. Defer despite cross-link evidence; image loading/alpha behavior affects both canvases. |
| [#4124](https://github.com/obs-rust/obs-studio/pull/4124) | Draft Linux PipeWire virtual mic/virtual camera sync optimization. Distinct from macOS physical input and CMIO output; defer until the real node/audio test actually executes and end-to-end A/V behavior is measured. |
| [#743](https://github.com/obs-rust/obs-studio/pull/743) | Experimental direct-output scaling; alias/overlap fixes and tests exist, but cross-platform evidence and speed measurements remain incomplete. Defer; no measured benefit may be claimed for dual outputs or masks. |
| [#742](https://github.com/obs-rust/obs-studio/pull/742) | Incomplete HDR mux metadata proposal: no encoder implementation, public API additions and unresolved licensing discussion. Defer; preserve standard recording behavior and ABI policy. |
| [#5721](https://github.com/obs-rust/obs-studio/pull/5721) | Frame-copy characterization overlaps merged #5741; do not repeat completed work or import a competing fixture blindly. Compare coverage and take only any still-missing assertions after upstream resolves it. |
| #5793, #5559, #4978, #4906, #4655 | CI/cache/test-discovery proposals; revisit only with a concrete fork need. #5793 strengthens Flatpak cache evidence and belongs with a future #5784 adoption. |

The remaining reviewed titles in this snapshot are included in the grouped rows above; this is a relevance triage, not approval of unmerged work. New upstream commits or PR revisions require another comparison.

## Code comment requirements for implementation

Add useful comments where upstream behavior meets Simulcast ownership, routing or compatibility decisions. Preserve existing correct comments; expand them only when needed. Comments should explain why the invariant exists, who owns the object, what is shared, and which regression test guards it. Use an upstream PR/commit reference for a non-obvious compatibility behavior. Do not narrate every line or leave stale claims of safety.

Planned locations:

- `OBSBasic::CloneSourceWithFilters`: explain that the group owns independent filters but references the original capture; ordinary source duplication is not a substitute. Existing comments already describe this; retain them when integrating #5806.
- `OBSBasic::CommitVerticalProgram`: private layout snapshot vs shared source/filter state; preserve `OBS_SCENE_DUP_PRIVATE_REFS` and Preview/Program UUID roles.
- `UpdateCleanSourceRecording` / teardown: ephemeral canvas ownership, fixed starting dimensions, normal track-1 audio and stop order; no implied isolated source audio.
- `audio_output.rs::audio_thread`: scheduler startup before the timing origin, permission-denied fallback and why #5791's syscall is needed for the package floor. Keep upstream's existing scheduler comments.
- `audio_callback.rs::audio_callback`: master gain is applied to mixed program tracks once, before the encoders consume them; changing thread priority must not duplicate or bypass gain.
- C/Rust source/filter adapters: native layout remains C-owned; hidden projection APIs avoid a second ABI layout; explain retain/release and signal re-entry constraints where needed.
- Future #5810 adoption: retain its owning-UUID and queued-callback lifetime comments; do not rework the master slider.
- CI workflow edits: explain fork-specific hosted fallback and required coverage; never leave upstream infrastructure assumptions unexplained.

Examples describe intended integration comments, not code already added by this planning change.

## Validation and release gates

Every implementation batch must update the living reference with exact imported SHAs, decisions, source functions, CI links, package links, limitations and rollback. Run compilation, regression tests and packaging in GitHub Actions; maintainer compilation is not required.

1. Source/ABI: existing Rust tests, original C tests against hybrid/reference, differential tests, layout/export checks and an independently compiled stock OBS plugin fixture. Never weaken a test to make the import pass.
2. Simulcast: horizontal and portrait Preview/Program, two destinations per orientation including failure/reconnect, scene/profile reload, shared-source clones, transformations/reordering, master gain, normal/paired/clean recording, API/hotkeys, shutdown and simultaneous output capacity.
3. Platforms/packages: Fedora RPM/AppImage, Ubuntu/Debian DEBs, Apple Silicon browser DMG; Windows builds where supported. Preserve actual platform availability, minimum runtime dependencies, credits, licensing and AI disclosure. Keep security/signing requirements.
4. macOS: physical camera permission/enumeration/capture/reconnect separately from CEF rendering/audio and signed/notarized virtual-camera activation. A browser-only ad-hoc package is not a virtual-camera validation.
5. Performance: measure mask cadence, webcam-to-output latency, audio gaps and A/V sync under load. Separate CPU and GPU filters; upstream scheduler benchmarks are not Simulcast hardware results.

Keep each integration PR independently revertable and leave master/alpha untouched until its gates pass. Record an explicit reason for each deferral; revisit on new evidence rather than restarting working subsystems.

## Complete merged-backlog ledger

Batch keys: A portable CI; B source/frame ports; C scene/filter/clone lifecycle; D audio; E conditional infrastructure/packaging. Classification is a proposal; none of these commits is imported by this document.

| Upstream PR | Commit | Change | Batch |
|---|---|---|---|
| [#5657](https://github.com/obs-rust/obs-studio/pull/5657) | `6cd5eb95bbc9` | obs-source: port obs_get_source_defaults to Rust | B |
| [#5637](https://github.com/obs-rust/obs-studio/pull/5637) | `76857f704883` | obs-source: port source frame initialization to Rust | B |
| [#5615](https://github.com/obs-rust/obs-studio/pull/5615) | `d36dfcb7773c` | ci: reclaim root-owned Tier 2 test scratch (Refs forge-ci/gh-runner#163) | A |
| [#5552](https://github.com/obs-rust/obs-studio/pull/5552) | `8f0ff71a7cad` | ci: add a canary for the pool signal that pool-select reads | E |
| [#5607](https://github.com/obs-rust/obs-studio/pull/5607) | `5578a3e695c8` | ci: put the app builds on the fleet cache (two lanes, inert without a URL) | E |
| [#5739](https://github.com/obs-rust/obs-studio/pull/5739) | `4c733bab7783` | obs-source: swap obs_source_update to Rust | B |
| [#5743](https://github.com/obs-rust/obs-studio/pull/5743) | `a3bfdd0accde` | ci: route merge queue setup away from hosted contention | E |
| [#5745](https://github.com/obs-rust/obs-studio/pull/5745) | `0e7df5262467` | ci: defer attested Tier 2 cross-links to the merge queue | E |
| [#5747](https://github.com/obs-rust/obs-studio/pull/5747) | `7e2f115eb109` | ci: report observed runner placement next to queue wait | A |
| [#5746](https://github.com/obs-rust/obs-studio/pull/5746) | `b2989494d800` | obs-source: port obs_source_draw and obs_source_draw_set_color_matrix to Rust | B |
| [#5748](https://github.com/obs-rust/obs-studio/pull/5748) | `c268c02b8975` | ci: require a ready Forge lease before routing Event Data | E |
| [#5749](https://github.com/obs-rust/obs-studio/pull/5749) | `7f54bf96e78b` | ci: keep Forge dependency cache checkpoints compatible | E |
| [#5754](https://github.com/obs-rust/obs-studio/pull/5754) | `62b110ae528c` | ci: skip Clippy and Dylint in the merge queue | E |
| [#5741](https://github.com/obs-rust/obs-studio/pull/5741) | `dc65e2371348` | obs-source: port obs_source_frame_copy to Rust | B |
| [#5755](https://github.com/obs-rust/obs-studio/pull/5755) | `4befe8804b76` | ci: link Windows test binaries once per merge group | E |
| [#5753](https://github.com/obs-rust/obs-studio/pull/5753) | `8c8beac39ca6` | perf(ci): batch Tier 2 timing tests on reserved CPU pairs | E |
| [#5759](https://github.com/obs-rust/obs-studio/pull/5759) | `33a4d9b9a9a2` | ci: try the verified local Tier 2 handoff first | E |
| [#5752](https://github.com/obs-rust/obs-studio/pull/5752) | `1a371e8bc576` | obs-source: port obs_source_output_cea708 to Rust | B |
| [#5761](https://github.com/obs-rust/obs-studio/pull/5761) | `8a8e7b63feb4` | obs-source: port the six async-frame entry wrappers to Rust | B |
| [#5764](https://github.com/obs-rust/obs-studio/pull/5764) | `69df7dfd1d68` | fix(ci): explicitly restart the Linux Tier 2 cache daemon | A |
| [#5765](https://github.com/obs-rust/obs-studio/pull/5765) | `152c528e41cb` | obs-source: port obs_source_show_preloaded_video to Rust | B |
| [#5767](https://github.com/obs-rust/obs-studio/pull/5767) | `e8cbea4dd014` | obs-source: port obs_source_get_last_async_ts to Rust | B |
| [#5774](https://github.com/obs-rust/obs-studio/pull/5774) | `6333e3aeea84` | obs-media-io: register the audio thread as a profiler root with its tick period | D |
| [#5778](https://github.com/obs-rust/obs-studio/pull/5778) | `8c38d3b98399` | ci: install the stock Tier 2 closure through the delta's recovery | A |
| [#5781](https://github.com/obs-rust/obs-studio/pull/5781) | `cef8f1da1af3` | obs-media-io: real-time audio thread on Linux through the Realtime portal or rtkit | D |
| [#5782](https://github.com/obs-rust/obs-studio/pull/5782) | `d32ece9a99f7` | docs(ci): record the ccache→zccache migration state (Windows hold, Flatpak options) | E |
| [#5783](https://github.com/obs-rust/obs-studio/pull/5783) | `a585d76f0eb2` | obs-media-io: time-constraint policy for the audio thread on macOS | D |
| [#5780](https://github.com/obs-rust/obs-studio/pull/5780) | `7ca20456e78c` | ci: give the hosted Windows Tier 2 job 40 minutes | A |
| [#5776](https://github.com/obs-rust/obs-studio/pull/5776) | `5fd85c15ec02` | obs-source: port obs_source_filter_set_order to Rust | C |
| [#5758](https://github.com/obs-rust/obs-studio/pull/5758) | `529d2d9fd151` | obs-source: port obs_source_get_frame and obs_source_release_frame to Rust | B |
| [#5789](https://github.com/obs-rust/obs-studio/pull/5789) | `246c4dee6b08` | obs-scene: port obs_sceneitem_set_order to Rust | C |
| [#5791](https://github.com/obs-rust/obs-studio/pull/5791) | `0ce0eb4f10e9` | obs-media-io: read the audio thread id through SYS_gettid | D |
| [#5794](https://github.com/obs-rust/obs-studio/pull/5794) | `5e2b9df81299` | obs-source: port obs_source_copy_filters and obs_source_copy_single_filter to Rust | C |
| [#5784](https://github.com/obs-rust/obs-studio/pull/5784) | `b60190ad9835` | ci(flatpak): cache OBS native compiles with zccache (replace ccache in the SDK lane) | E |
| [#5796](https://github.com/obs-rust/obs-studio/pull/5796) | `c4c8e3869d1d` | obs-source: port obs_source_filter_remove to Rust | C |
| [#5798](https://github.com/obs-rust/obs-studio/pull/5798) | `b152594daca3` | fix(ci): AI rebase treats an unreadable queue as closed | A |
| [#5800](https://github.com/obs-rust/obs-studio/pull/5800) | `28261c4c8be2` | obs-source: port obs_source_filter_add to Rust | C |
| [#5804](https://github.com/obs-rust/obs-studio/pull/5804) | `165c424af004` | obs-source: port obs_source_remove to Rust | C |
| [#5806](https://github.com/obs-rust/obs-studio/pull/5806) | `7b47a985ba6c` | obs-source: port obs_source_duplicate to Rust | C |
| [#5808](https://github.com/obs-rust/obs-studio/pull/5808) | `723cd1c803bd` | fix(ci): AI rebase capacity-recovery treats an unreadable PR list as nothing to requeue | A |
