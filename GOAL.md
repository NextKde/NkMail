# NkMail local delivery goal

## Flatpak KWin controls and sidebar follow-up · 2026-11-10

- User reported an application-drawn region over KWin-owned traffic lights, folder icon/title alignment, and insufficient hover spacing. Preserve the window contract: publish button geometry to KWin; hide client fallback when the compositor acknowledges ownership.
- Confirmed released v0.1.0 omits the `Kos.SurfaceShape` module required by NativeWindowControls.qml; its dynamic-load failure leaves client buttons visible. Existing X11 lifecycle checks and mocked takeover tests did not cover this real integration.
- Prepare 0.1.1 with the canonical module pinned to NextKde/NextKde d7b34a8e8b37a9fb7605333f10d02f7b16e70743, built against the existing shared KDE 6.11 runtime. No upstream source edits or host plugin binaries. Screenshot acceptance must load the real module, plus a dedicated Wayland/KWin takeover check.
- Work is isolated in codex/flatpak-kwin-controls. Existing uncommitted icon changes in the primary checkout are unrelated and preserved. All builds and execution for this follow-up are VM/CI-only.
- The existing VM desktop bridge lacks the controls protocol. Built the pinned canonical bridge in the VM and launched it in an isolated nested KWin; no running desktop replacement. Real module loading and compositor takeover now pass for inbox and composer. Missing-module and absent-compositor review checks fail closed. Native full suite passed 11/11 (132.70 s); sidebar capture check passed 3/3. A review-only false negative caused by selecting the hidden composer from the inbox QObject tree was fixed by filtering by QQuickWindow.
- PR #38 carries the fix. First candidate 7175a54 passed release-gate and Flatpak lifecycle; final candidate 34fb749 includes the corrected per-window review gate. Final 34fb749 passed release-gate/Arch run 42846541027 and Flatpak lifecycle run 42846541152. The exact downloaded bundle passed real KWin v2 takeover checks and stable compositor captures for inbox and composer in the isolated VM; it was then uninstalled with data retained. Bundle SHA-256 dbd3094e0baf91b99220bb679bc086002d9f11bf5c579bbd6c8c2c5527b32739. Publication of v0.1.1 follows protected PR #38 integration; consult its live state and Release before resuming. See docs/progress/2026-11-10-kwin-controls for acceptance evidence.

## Application artwork · 2026-11-10

- Replaced `data/nkmail.svg` with the shared original NextKde icon family: soft
  blue tile, ivory envelope, fine seam and restrained depth. Qt SVG export and
  transparent-corner checks passed in the dedicated VM. No host app was launched.
- This is source artwork only; published v0.1.0 bundles remain unchanged. Catalog
  PNG synchronization is delivered separately in NextKde/appCatalog. Normal PR
  release-gate checks still govern main integration.

## Flatpak and appCatalog integration · 2026-11-09

- Delivery complete on 2026-11-10: stable [NkMail v0.1.0](https://github.com/NextKde/NkMail/releases/tag/v0.1.0) published by [run 42732344261](https://github.com/NextKde/NkMail/actions/runs/42732344261). All 10 GitHub assets match the accepted local artifact hashes/sizes; the tag resolves to accepted source 4ff223d6453af784f310426783f1df6577f7dcc1. The Release is latest, non-draft and non-prerelease.
- [appCatalog PR #3](https://github.com/NextKde/appCatalog/pull/3) is ready for review and its catalog CI passed. The user handles catalog signing; no new signed catalog was published by this task. Packaging/publisher PRs #33–#35 merged through the required release-gate. No remaining application publication blocker.

- User requested a Flatpak following kos-apps and publication/submission to appCatalog. Shared runtime: org.kde.Platform//6.11; WebEngine supplied by matching BaseApp, without host Qt bundling. Canonical UI pin unchanged.
- Added reproducible packaging, schema-2 release metadata, source exports, sandbox identity/permissions, and disposable-CI synthetic lifecycle checks. Final source 4ff223d6453af784f310426783f1df6577f7dcc1 passed acceptance; PR #33 merged through the required checks.
- User will handle the final catalog signature. Scope here is the NkMail stable Release and appCatalog PR, after required release-gate and package validation. No live mail operation authorized or performed.
- NkMail PR #33 opened. First package run 42631129624 built Boost/QtKeychain but mailio rejected the default in-source CMake build; set `builddir: true` before retrying. Catalog entry validated in the designated VM: 6 entries, 3/3 tests passed.
- Run 42631957886 successfully built/exported/installed the Flatpak and produced three offline synthetic screenshots. Its same-commit `--reinstall --bundle` check failed with “already installed”; replace with explicit uninstall/reinstall and verify data survives both removals. Add normal-launch single-instance acceptance and desktop portal services to the disposable runner. Preserve successfully built candidates as CI artifacts even if smoke acceptance fails; never publish a failed candidate.
- Both initial release-gate jobs passed. Preserve the older Arch preview artifact filename by passing its explicit `--version 0.1.0rc1` to the generalized exporter.
- Candidate 8526746: full release-gate and Arch package succeeded; app bundle built and three offline screenshots inspected successfully. Normal-launch test correctly caught a packaging bug: the later `--talk-name=org.NextKde.NkMail` overrode `--own-name`, confirmed in the actual bundle metadata. Remove duplicate talk and require `own` in the producer's metadata check. Also pass XAUTHORITY to the CI portal activation environment and retain primary-process diagnostics.
- Final acceptance: run 42646453656 passed 32 dependency tests, all 11 application suites (98.23 s) and the Arch package; run 42646453663 passed exact-bundle install, three offline previews, normal-launch single-instance activation, reinstall and data-preserving uninstall. Downloaded final metadata and all three screenshots were verified. The rejected 8526746 candidate is not a release asset.
- Accepted bundle: 134,776,112 bytes; installed size 511,931,904 bytes; SHA-256 49ebe362ae8acd62ad6fdd38303d77b2bf4aadddedf12dc781430af0274bfd14; OSTree d481ce8951b85e1e0b2252ae4daff5301d514687950bd2be30f787b94dc89823. Shared runtime org.kde.Platform/x86_64/6.11. No embedded origin/runtime-repository URL. Temporary diagnostic app installations were removed.
- Delivery locations: https://github.com/NextKde/NkMail/releases/tag/v0.1.0 and appCatalog branch codex/nkmail-flatpak. Consult their live Release/PR states before resuming; never overwrite a published tag. Large local uploads were slow, so a manual publisher resumes identical draft assets directly from accepted GitHub CI artifacts, with source ancestry, workflow and bundle checks. The user retains catalog signing/publication.
- Publisher PR #34 merged after release-gate passed. First publication run 42730788008 stopped before upload because Ubuntu's minimal `flatpak` package does not install the separate libflatpak shared library. PR #35 explicitly installed `libflatpak0` and added an immediate library-load check. It merged after release-gate passed; retry run 42732344261 then verified all assets and published successfully. Updated catalog entry validation passed in the VM (7 entries, 3/3 tests).

## Consolidated integration of PRs #7–#16 · 2026-11-03

- Created a single integration branch from current main (27c884a), folding in the full provider-presets stack through 2240831 as one change. The existing main sidebar-heading correction is retained. Original branches and PRs remain intact until integration is accepted.
- UI gitlink/lock remain e2592b2742c58c1bd797a8bcfa2a3ae7da4affd8. No component upgrade or real mailbox operation.
- Fresh default-source Release build in the VM succeeded. Dependency gate and all 32 dependency-guard tests passed; full CTest passed 8/8 in 76.23s (CLI, UI, i18n, catalogs, tray, core, controller and synthetic loopback protocol). No host compilation, GUI execution or tests for this integration.
- Integration PR: https://github.com/NextKde/NkMail/pull/17 targets main. Required remote release-gate remains the merge gate; do not close original PRs #7–#16 until this integration is merged.

## Composer public controls and glyph correction · 2026-11-03

- Unified all header action glyphs to 16px SVG slots with the same stroke treatment; removed checked/active fills from the formatting and Bcc header launchers. The header retains canonical liquid material; embedded formatting controls remain ordinary.
- Replaced the customized KosComboBox wrapper/background with canonical LiquidSelect in explicit solid trigger/popup modes, 24px height and 6px trigger corners. No custom select background or popup implementation remains. Shared keyboard/navigation/selection behavior is exercised by actual font/face/size selection tests.
- Fixed emoji cell clipping/elision using a centered 22px Noto Color Emoji content item and no label padding. All 18 entries inspected in the opened native Wayland picker; regression checks every cell fits the content area and preserves insertion/close behavior.
- Added isolated demo-only compose-emoji/font review scenarios; an explicitly requested demo scenario now opens in interactive preview too.
- VM build/UI/i18n/catalog passed 3/3 in 40.83s (47 UI cases). User-authorized host build and native Wayland light/dark/emoji/font captures inspected; automated tests remained VM-only. Canonical library gitlink unchanged; no real mailbox operations.

## Send action beside window controls · 2026-11-03

- Composer send action now follows the inbox search placement: 10px from its right edge to the visible first traffic-light circle, with matching vertical center. It no longer offsets from the oversized reserved-control region.
- Extended the independent-composer regression to measure the actual circle and send-action geometry at widths 640, 800, 1040 and 1400. VM build/UI/i18n/catalog checks passed (3/3, 31.10s; 46 UI cases); refreshed light/dark software-fallback screenshots inspected. No host validation performed.

## Compact small-corner formatting controls · 2026-11-03

- Corrected the misunderstanding of small corners as square corners: the entire formatting strip uses 24px-high ordinary controls with 6px corners. Emphasis, alignment, lists and indentation are grouped; all four select triggers, their popup panels and the color control share the small-corner treatment.
- VM build and existing UI/i18n/catalog checks passed (3/3, 30.79s; 46 UI cases). Refreshed light/dark software-fallback captures inspected. No host validation performed.

## Composer editing tools · 2026-11-03

- Continued from the supplied macOS Mail reference while retaining the NextKde window/material contract. Added text presets, installed font families and faces, selection-aware formatting state, paragraph alignment, numbered lists and block indentation. Added anchored emoji/link editors and body-only formatting shortcuts. Selection replacement is one undo operation.
- Added UI regressions for selected-block operations, saved HTML, indentation undo, unsafe link scheme rejection, selected-text links and emoji replacement/undo. Chinese and English catalogs updated.
- VM source directory: `/home/amao/nextkde-validation/nkmail-composer-20261103` on `kos-vm-dev`. VM build and UI/i18n/catalog checks passed (3/3, 30.21s; 46 UI cases). Chinese light/dark software-fallback captures reviewed. Wayland screenshot capture failed because the preview was not exposed; compositor rendering/liquid refraction are not visually accepted by these exports. No host build, test or GUI run performed for this iteration.


## Restore composer NextKde ownership contract · 2026-11-03

- Corrected the reference-driven composer implementation: removed application-owned 22px window corners and border, the leading custom decoration layout and disabled platform adapter. The composer now reuses the inbox WindowChrome/MailWindowControls cooperative path over flush rectangular content planes. Window shape and decoration stay with NextKde/compositor ownership.
- Floating header actions now use canonical liquid material with an explicit eligible same-window LiquidBackdrop. Embedded formatting controls remain ordinary material. Close confirmation again uses canonical LiquidDialog defaults and unmodified 30px LiquidDialogButton role styling; removed forced opaque mode and custom destructive tint.
- Recorded the top-level window/material contract in AGENTS.md. Scoped inbox-control tests to their own chrome now that both windows share the canonical component. Added assertions for rectangular content, enabled protocol component, and liquid-action backdrop ownership.
- Host build and UI/i18n/catalog checks passed (3/3, 15.89s; 43 UI cases). Synthetic Wayland composer and close-decision captures inspected; no capture runtime warnings. Client screenshot exports intentionally omit compositor-owned controls/corners. No KWin/reference code was modified or executed.

## Compact account setup and provider scrolling · 2026-11-03

- Account-dialog height now follows visible content up to the host-window limit, removing fixed-height blank space. Reduced header/content/footer padding and field spacing. Provider selection shows six rows with a persistent interactive scrollbar on the right.
- Host build and UI/i18n/catalog checks passed (3/3, 16.21s; 43 UI cases). Pointer-drag regression reaches and selects the last provider; compact height and right-side scrollbar placement are checked. Synthetic Chinese Wayland capture inspected with no runtime warnings. Host testing remains authorized for these NkMail iterations.

## Independent Mail-style composer · 2026-11-03

- Replaced the inbox modal composer with an independent, nonmodal, resizable ApplicationWindow. Reused canonical window controls/resize handles and solid capsule actions; added a leading window-control cluster, centered toolbar, circular send action, full-width address rows, horizontal formatting strip and unboxed expanding editor.
- Native window close and Escape/Ctrl+W offer Save / Don’t Save / Cancel. Save failure preserves the window; discard removes the local draft and stops autosave. Sending/pending send block close/discard; account mismatch disables editing and destructive actions. SMTP acceptance hides only the matching composer.
- Formatting uses Qt TextSelection for current selection/insertion styling; list operations now receive the actual editor selection. Font family/size, text color and strikethrough join the existing bold/italic/underline/list actions. Synthetic compose capture now targets the separate window.
- User explicitly authorized host testing for this composer iteration in this chat. Host build and UI/i18n/catalog checks passed (3/3, 15.76s; 42 UI cases). Selected-text formatting, independent resize/minimize, close/save failure, cancel and autosaved-draft discard are covered. Chinese light/dark and close-decision Wayland captures inspected; no runtime warnings in capture logs. Earlier Row-layout and offscreen raise issues were corrected before the passing run.

## Local draft deletion entry · 2026-11-03

- Exposed the existing account-scoped discardDraft operation through a trash action on each local draft, with cancel/confirm dialog and Chinese/English translations. Sending, pending send and account changes block confirmation; failed deletion keeps the dialog open.
- UI regressions for cancellation, targeted deletion, send guards and stale-account confirmation passed with the composer suite after the user explicitly authorized host testing in this chat.

## Pointer-positioned message context menu · 2026-11-03

- Message right-click menus now use TapHandler pointer coordinates instead of anchoring below the whole row. Context menus omit the anchor arrow and clamp inside the overlay; Move-to uses the saved context point. Non-pointer review invocation falls back to row center.
- UI regression passed (12.44s), including real right-clicks at distinct positions and bottom/right boundary placement. Synthetic Wayland capture inspected.

## More menu empty first row · 2026-11-03

- The compact-only Display item was invisible on wide windows but retained its 30px MenuItem height. Explicitly collapse its height with the compact-options condition. Shared Kos.Ui stays unchanged.
- Wide/narrow/wide regression checks the first visible row position after the ListView layout settles. UI suite passed (12.25s); isolated Wayland More-menu screenshot inspected.

## Editable demo email address · 2026-11-03

- Removed demo-only readOnly from the email field so the interactive preview accepts typing and detects providers. Submission lock remains; demo credentials/network/send restrictions remain in place.
- Keyboard-entry regression verifies typed Gmail address and automatic preset selection. UI suite passed (11.46s); rebuilt interactive preview replaced.

## Account address input layout · 2026-11-03

- Removed the indented SettingsGroup label/row for email. A left-aligned heading now sits over one full-width shared text field, with Chinese placeholder `请输入you@example.com`. The same address field continues to drive provider selection and saved configuration.
- Host build, UI, i18n and translation checks passed (3/3, 11.07s); synthetic screenshot inspected.

## iCloud logo clarity · 2026-11-03

- Replaced the low-resolution ICO with Apple’s 180×180 touch icon, resolving its URL against the iCloud page’s HTML base. Display geometry stays unchanged; the bundled asset remains offline. Host build and UI regression passed (11.19s); updated screenshot inspected.

## Provider icon sizing correction · 2026-11-03

- Unified the icon column to 28px and enlarged Other's envelope to 28px. NetEase wordmarks now show only the numeric brand portion within a 24px square tile, removing long taglines and domain names.
- Host build and UI regression passed (10.93s); updated synthetic light screenshot inspected.
- iCloud is an IMAP/SMTP app-specific-password preset, not an Apple Account browser sign-in integration. No real iCloud account has been validated; official setup requirements were rechecked against Apple support.

## Provider logo follow-up · 2026-11-03

- User requested each provider's own logo. Added local provider identity assets, using shared LiquidNavigationButton behavior with an app-owned content layout to preserve brand colors and wide NetEase wordmarks. Other keeps its generic envelope; sources and rights are recorded in data/providers/README.md. No runtime provider requests or shared component edits.
- Host build and UI suite passed (11.14s), including decoded packaged logo checks for every provider. Catalog/i18n checks and canonical dependency gate passed. Chinese/light and English/dark synthetic Wayland screenshots inspected; wide NetEase logos receive a light backing in dark mode for contrast. Follow-up targets PR #16.

## Provider presets and protocol compatibility · 2026-11-03

- Continued the interrupted provider-presets task. Added independent ProviderPicker/ProviderProfiles for iCloud, Gmail, QQ, Tencent Exmail, 163, 126, Yahoo, AOL and manual IMAP/SMTP. Unknown custom domains remain manual. Microsoft/Exchange is visibly unavailable; OAuth registration and implementation remain deferred by user choice.
- Existing AccountSetup, MailTransport and SendService remain responsible for connection verification and mail operations. Added capability-gated static IMAP ID, server special-use Sent/All roles and a persisted server-managed Sent-copy option (enabled by the Gmail preset). No real mailbox writes or SMTP submissions were performed.
- Full host CTest passed 8/8 in 44.98s on takeover. Synthetic Chinese Wayland screenshot inspected; protocol fixtures cover 44 loopback/TLS scenarios. Provider behavior, optional-ID refusal, special Sent selection and no duplicate Sent copy have regressions. Dependency gate and 32 guard tests passed. Remote PR/release-gate is still required; do not imply live-provider verification or completed OAuth.
- Delivery details and limitations: docs/PROVIDERS.md and docs/progress/2026-11-03-provider-presets/README.md.

## Sidebar reference hierarchy correction · 2026-11-03

- The earlier completion statement overstated the visual scope: account folder sections alone did not match the reference navigation. Added always-visible Favorites (Inbox, Flagged, Local drafts, Sent), custom pins, Smart mailboxes (Unread, With attachments) and expandable account folders identified by address. Only the actual navigation origin is highlighted.
- Smart queries include all cached folders of the current account, retaining folder/UIDVALIDITY/UID and pagination/search semantics. This is explicitly not a cross-account unified inbox. Reading routes to the original mailbox; existing command service rejects mixed-folder batches before any mutation.
- Added account/cache isolation, stale validity, pagination, duplicate-UID source routing, mixed-folder rejection and sidebar interaction regressions. Full host CTest passed 8/8 in 43.80s. Synthetic Wayland navigation and Flagged captures inspected. Shared liquid source/material/radius unchanged.

## Confirmed first-stage delivery complete · 2026-11-03

- Scope: the user-confirmed three-row table plus reader/details reference. Implemented in PRs #8–#13, with the final wizard busy-save retry fix following #13. Optional local initials, grouped reply/forward and batch targeting, email-first verified setup, account-scoped navigation/roles/status, compact reader and inline details are all implemented.
- Final audit: save rejection now always releases wizard submission state, preserves non-sensitive configuration and supports retry. Its regression passed in the final host UI run (9.84s). Full suite before this focused QML fix passed 8/8 in 43.20s; dependency gate and 32 guard tests passed. Foundation PR #7 remote release-gate passed. Stacked PRs are pending review/merge, not represented as merged or independently CI-approved.
- Screenshots are synthetic application captures from the host. They preserve liquid controls, shared pinned source and compact geometry; compositor-owned traffic lights are not captured. No live provider authentication or real mailbox mutations were performed.
- Later-phase product ideas below (OAuth/Exchange, unified inbox, copy/attachment-forward, colored flags/reminders/rules) remain explicitly deferred, not delivered by this first-stage work. Current provider preset is iCloud; other providers use manual IMAP/SMTP.
- Per-feature matrix and screenshots: docs/progress/2026-11-03-completion/README.md.

## Compact reader · 2026-11-03

- Independent MailMessageHeader presents sender, subject, recipient, source, time and inline details. Narrow layouts stack safely; long subjects wrap. ReaderLayout scales explicitly loaded data images using dimensions provided by RemoteImageLoader, preserving aspect ratio and blocked remote placeholders. Existing sanitizer and attachment behavior remain.
- Host full CTest passed 8/8 in 43.20s, including long-header and image sizing regressions. UI dependency gate and 32 guard tests passed. Chinese/light and English/dark synthetic Wayland screenshots inspected; compositor traffic lights are outside these captures.
- Final audit found a wizard save rejection could leave submitting true when mailbox work is busy; follow-up fix is next.

## Account-scoped mailbox navigation · 2026-11-03

- Background/favorite unread badges read account-scoped cached counts. Added independent navigation ordering, server special-use role preservation/storage (schema v5), explicit local/server drafts and transient account sync/error text. Same-named folder identities remain distinct.
- Seven full-run suites passed; the existing folder-role count assertion was updated for the new field and core rerun passed (0.43s), including new account-scoped cache coverage. Protocol special-use and UI ordering tests passed. Screenshot inspected. Continuing compact reader header.

## Account setup wizard · 2026-11-03

- Email-first setup, iCloud/manual profile choice, collapsed advanced settings, independent IMAP/SMTP verification and explicit save completion. ProviderProfiles and AccountSetup own their respective logic. Failed verification retains fields, clears password, and permits retry. Saving reports success explicitly.
- Local full suite 8/8 passed in 41.87s; final UI/catalog/protocol checks 3/3 passed in 28.09s. Auth-only loopback checks assert no message submission. Synthetic local screenshot inspected. Continuing navigation and compact reader.

## Optional sender initials · 2026-11-03

- Added locally generated initials behind a Display option with per-profile persistence, preserving compact row height and selection geometry. Local CLI/UI/catalog checks passed; Wayland synthetic screenshot inspected. Continuing account wizard, navigation and reader header.

## Message context actions · 2026-11-03

- Added reply/reply-all/forward group to the existing liquid context menu. Compose actions require matching loaded UID and UIDVALIDITY and are unavailable during sending or multi-selection. Existing command service handles single/batch read, flag, move, archive and delete through the same bulk entry points. Right-click outside the selection clears stale batch targets.
- Local UI and catalog regression passed 2/2 in 8.66s, including multi-selection targeting and stale/missing content gating. Synthetic Wayland menu screenshot inspected; no shared style changes.
- Remaining planned work: optional list avatars, account setup wizard, navigation counts/roles and on-demand sync status.

## Reader-local details entry · 2026-11-03

- First incremental feature: removed toolbar details cell, added canonical inline action beside message time, and moved the existing details panel below the mail header. No shared UI material or radius changes.
- User now authorizes host testing/screenshots. Local Wayland synthetic captures are included; application captures omit compositor-drawn traffic lights. Local UI interaction regression activates the new details entry. Full test results are reported in the PR.
- Foundation PR #7 holds prior reviewed work; this feature is stacked on that branch. Next: compact reader header and unified message action menus.

## 下一阶段产品目标：邮件操作、账户接入与导航一致性 · 2026-11-02

以下是最初的差距评估与分阶段规划；阶段一现已完成，最新交付状态见本文顶部。参考图只作为信息层级与交互参考；不复制截图内真实邮件、地址或密钥片段。

目标：在保留紧凑三栏布局、液态按钮组、6px 选中圆角、中英文和单实例托盘行为的前提下，让用户能够清楚地添加账户、定位邮件、执行常用操作，并确保每个可见入口对应真实能力。保留已确认的标题栏居中、4px 下移和搜索距交通灯10px布局。

现状与差距：
- MessagePane 已有紧凑邮件行、未读点、蓝底白字选择、多选、线程折叠、附件/旗标提示和基础右键菜单。回复/全部回复/转发已存在于其他入口，需统一到行菜单并锁定目标邮件；发送者头像尚未呈现。复制到、附件转发、彩色旗标、提醒、静音、屏蔽及规则需要额外后端或本地数据模型。
- AccountDialog 目前直接展示身份、密码、IMAP/SMTP、TLS与高级配置，缺少邮箱优先的分步接入和提供商预设。保存配置不能等同于认证成功。现有应用密码接入不能冒充 OAuth 或 Exchange 原生支持。
- MailSidebar 已有收藏、按账户展开文件夹、账户管理和本地草稿。收藏无计数，非当前账户文件夹未读数被置零；本地草稿入口与账户文件夹层级需整理。系统文件夹显示/icon仍有名称映射逻辑，需以服务器角色优先，避免混淆同名或不同路径文件夹。缺少统一收件箱/有旗标等聚合视图。

阶段一：整理现有能力（优先交付）
1. 邮件行：提供可选的本地生成首字头像，不请求远程头像；保持紧凑行高、内容留白、单行摘要与轻分隔线。
2. 操作菜单：按打开/回复转发、已读旗标、归档移动删除分组；复用已有命令。右键未选中邮件时明确操作对象，多选显示匹配的批量能力，禁用状态有依据。为危险操作保留现有语义，不擅自扩大删除范围。
3. 账户向导：邮箱入口 → 适用的预设/手动配置 → 分项连接验证 → 明确成功或失败。高级端口/TLS设置渐进展开；错误可重试且保留非敏感输入。未知域名回到手动设置，预设和认证方式需在实现时核实提供商官方信息。
4. 导航：统一分组标题、图标列、计数列和选中状态；系统文件夹排序和角色映射独立建模。保留不同服务器文件夹身份，不按翻译后的同名合并。明确本地草稿与服务器草稿的区别。同步状态仅在进行中/失败时按账户显示，不恢复永久底栏。

5. 邮件阅读区（补充参考图）：将“详情 / Details”从顶部工具栏迁入邮件头右侧，作为可键盘访问的轻量文字按钮；复用现有详情数据与展开/收起能力，常驻显示发件人、主题、收件人摘要和时间，详细地址、抄送等按需展开。移除工具栏对应按钮并同步收紧按钮组，不留空位。
   - 把当前大主题、头像/发件人、时间的纵向堆叠整理成紧凑邮件头：左侧头像与身份信息，右侧账户/文件夹、时间和详情入口；窄窗口自然换行，长主题/地址不挤压按钮。
   - 邮件头和正文用轻分隔线区分；统一正文左右留白、段落行距、附件区域与图片自适应宽度。保留现有正文净化和远程图片显式加载策略，不能为模仿截图直接加载远程资源；富文本能力按现有安全渲染器实际支持范围验收。
   - 补充验收：展开/收起详情、切换邮件、长收件人列表、无主题、附件/内嵌图、窄窗与中英文。该项纳入阶段一，与操作菜单和导航共用既有服务，不新增第二套邮件状态。

阶段二：新增日常高价值能力
- 统一收件箱、未读、有旗标等只读聚合视图；所有行必须携带账户+文件夹+UIDVALIDITY+UID，打开/操作必须路由回原账户，不依赖当前账户猜测。
- 收藏排序和持久化、后台账户缓存计数；标明未知/过期状态，不能把未同步当成零。
- 增加移动到垃圾邮件与复制到，基于明确的文件夹角色和协议能力。附件形式转发需要保留原始 MIME、正确生成 message/rfc822 并做回归。

阶段三：独立评估，不以空菜单交付
- OAuth、Exchange 原生接入；需逐提供商核实授权注册、协议和令牌生命周期。
- 彩色旗标（标准 IMAP Flagged 只有布尔值，颜色需独立存储并明确同步范围）、提醒、会话静音、发件人屏蔽/分类和规则引擎。
- 再次发送与代转分别定义语义和权限，不能简单复用普通转发。

模块边界：
- QML 负责视图、输入和状态展示，不直接连接邮箱。
- 新的 MessageActionModel/组件统一菜单、工具栏和快捷键的命令描述，执行仍由 MailboxCommands、DraftService、SendService 负责。
- ProviderProfiles 与 AccountSetup 状态机分别负责预设和接入流程，AccountSession 负责账户会话，凭据继续使用系统钥匙串。
- MailboxNavigationModel 负责文件夹角色、分组、收藏和计数；聚合查询单独实现，不把遍历多账户和协议逻辑塞进 MailController/Main.qml。

验收：
- 先完成阶段一的三块界面及真实操作，再开展阶段二；每块提供合成数据预览，按用户反馈微调。
- 覆盖窄/默认/宽窗口、中英文、明暗主题、空列表/长文本/多账户与错误状态；已确认的紧凑布局不退化。
- 验证单邮件与多选的操作目标、跨账户路由、异步期间切换账户、重复提交和失败重试；无可用能力时不显示可执行的假入口。
- 自动化、协议与图形回归在虚拟机完成；本机仅在用户已授权范围编译打开预览。账户写入/发送测试仅使用合成 loopback 数据，不操作真实邮箱。
- 每阶段独立提交，记录结果和未验证项；不得以软件渲染检查代替真实合成器验收。


## Inbox top separator · 2026-11-02

- Suppressed the first message row top separator so it does not overlap the inbox header rule. Inter-row separators retain their existing appearance. VM UI regression passed in 11.21s; host build and guarded preview replacement succeeded.

## Search spacing refinement · 2026-11-02

- Increased search-to-traffic-light spacing from 5px to 10px for both icon and input. VM UI regression passed in 11.32s, with half-pixel layout-rounding tolerance. Host build and guarded preview replacement succeeded.

## Search beside traffic lights · 2026-11-02

- Removed cumulative toolbar/search trailing margins; search now ends at most 5px before the visible first traffic-light circle. Moved compact-search close action inside the input with reserved text padding so the input itself reaches this edge.
- VM UI passed in 11.36s, covering icon/expanded/wide input at four widths. RowLayout pixel rounding makes the gap slightly under 5px; assertions enforce the requested maximum rather than exact fractional placement. Host build and guarded preview replacement succeeded.

## Header layout and four-pixel offset · 2026-11-02

- Set the control centre to 23.5px, 4px below its original position, and centre all titles/controls within a 47px header. List, reader, sidebar and dividers follow the header bottom. Replaced rounded centre anchors with exact centre expressions after VM assertions exposed a half-pixel mismatch.
- VM UI regression passed in 10.44s, including title/control centre alignment and content geometry. Host build and guarded preview replacement succeeded.

## Refined header offset · 2026-11-02

- Reduced requested downward offset from 10px to 6px (4px upward from previous preview), retaining the common alignment. VM UI regression passed in 10.32s; host build and guarded preview replacement succeeded.

## Header controls vertical offset · 2026-11-02

- Moved traffic-light row down 10px. Existing shared center-line bindings move toolbar groups, search, filter, folder title and sidebar toggle together; header height remains 44px. Updated expected circle Y coordinate to 22.5px.
- Host build and guarded preview replacement succeeded. VM UI regression passed in 10.59s.

## Cooperative window controls · 2026-11-02

- Added an app-owned optional platform adapter for the pinned Kos.Ui controls, using actual delegate rectangles and `WindowControlSlot`. Native takeover hides/disables fallback only after active acknowledgement; loss restores it. No reference project or dependency pin changed. Calendar placement and 22px client hit cells now match.
- VM build and UI regression passed (10.44s), including exact circle coordinates, native-active simulation, restoration and missing-adapter fallback. No host validation performed.
- Attempted real VM Wayland capture: no window-controls protocol appeared in the trace, bridge debug returned no diagnostics, and capture failed because the window was not exposed. Real compositor/GPU acceptance remains unverified; no desktop configuration was changed. The existing host preview still runs the previous binary.

## Preserve inbox header during search · 2026-11-02

- Removed search-state coupling from the message-list header visibility. Opening search only replaces reader-toolbar actions; the mailbox title and unread filter remain visible.
- Added assertions for title/filter visibility during compact search. VM UI regression passed in 9.98s; host build and guarded single-instance preview replacement succeeded.

## Consistent toolbar sizing and compact search · 2026-11-02

- Unified filter, sidebar, action groups and search to the shared 28px toolbar height. Visible groups retain horizontal labels; operations that do not fit move to existing menu commands. Search/back controls no longer use stacked labels. Compact options retain a View route through More.
- Compact search now exposes a visible input and close control; header actions give it room. Escape prioritizes active search over sidebar collapse. Updated geometry assertions and exercised compact typing/escape. Initial regressions exposed the old Escape priority; fixed without weakening the test.
- VM UI/catalog checks passed 2/2 in 10.12s. Host build succeeded and old preview exited through guarded tray quit before replacement; single instance retained.

## Top-only message separators · 2026-11-02

- Moved each message separator to the top of its row, leaving no trailing line below the final message. Preserved horizontal insets, lighter opacity and selected-row treatment.
- Host build and guarded single-instance preview replacement succeeded. VM UI regression passed in 9.67s.

## Joined toolbar hover surfaces · 2026-11-02

- Replaced independent capsule hover backgrounds inside toolbar groups with clipped segment highlights: rounded only at the two outside ends, square at internal boundaries, explicitly antialiased. Preserved the canonical shared liquid surface and keyboard focus ring. Rounded segment widths to whole pixels and disabled per-cell scaling.
- VM UI/catalog regressions passed 2/2 in 9.87s. A temporary offscreen hover capture exercised first/middle/last segments; inspected the captured hover geometry (software rendering, not GPU acceptance). Initial capture harness lacked visible:true and incorrectly treated save() as returning a bool; corrected the harness and reran successfully.
- Host build succeeded and the prior preview exited through guarded tray quit before replacement; single-instance preview retained.

## Softer row edges · 2026-11-02

- Reduced message selection/hover corner radius to 6px. Separators now render at half their existing theme opacity, preserving light/dark colors and horizontal insets.
- Host build and guarded single-instance preview replacement succeeded. Final VM UI regression passed in 9.48s.

## Compact inset message rows · 2026-11-02

- Reduced message row height from 84 to 68 and vertical content padding to 7px, with a single elided summary line. Text and separators now have 24px left/right insets. Selection/hover surfaces have 8px horizontal insets and 8px corners; active white text is preserved.
- VM UI and translation guards passed 2/2 in 9.40s; inspected the 1180×760 Chinese synthetic screenshot. Host build succeeded and the prior preview exited through the guarded tray menu before replacement.

## Chinese/English localization · 2026-11-02

- Added LanguageService with profile-scoped language preference, system default, English/zh_CN selection, embedded Qt catalogs, live QQmlEngine retranslation and optional installed Qt dialog catalogs. More exposes all three choices; --language supports isolated reviews. Demo preferences remain temporary.
- Completed 353 entries in each catalog, covering QML, tray, account/draft/send status and application-owned protocol/storage errors. Restored extraction for SendService callbacks. Standard folders and dates follow the selected locale; user mail/custom server text is not translated. Already-generated operation messages can retain their original language until a new status update.
- VM full seven-suite regression passed in 45.21s before the catalog guard/native-tray test extension; final i18n + source/catalog guard passed 2/2 in 0.15s, including live native tray-menu translation, QML retranslation and saved choice. There are now eight suites required by CI. English and Chinese 1180×760 synthetic screenshots were inspected; no GPU-material claim.
- Host build succeeded and the prior preview exited through its guarded tray menu before opening the new Chinese preview. Single interactive instance retained; no real mailbox or reference project was used. No host automated tests.

## Header alignment and spacing · 2026-11-02

- Aligned the unread filter to the shared window-control/toolbar center instead of its independent 44px row. Reduced shell header height from 68 to 44, removing 24px of unused space above the list and bringing all panes upward together.
- VM UI regression passed in 9.69s, including an explicit filter/toolbar center alignment assertion and updated header geometry checks. Inspected the 1180×760 synthetic screenshot. Host build succeeded; replaced the previous preview via guarded tray exit, retaining one instance.

## Smaller default window · 2026-11-02

- Reduced the initial window from 1280×820 to 1180×760, retaining the compact 189px sidebar and 280px message list. Saved user window sizes remain respected.
- Host build succeeded; replaced the previous synthetic preview through guarded tray exit and opened at defaults. VM UI regression passed in 9.38s.

## Compact initial message list · 2026-11-02

- Reduced the message list default/reset width from 320 to 280; retained drag resizing, the 260 minimum and saved custom widths. Updated keyboard/reset geometry expectations.
- Host build succeeded; previous preview exited through its guarded tray menu before replacement. VM UI regression passed in 9.43s.

## Narrower navigation sidebar · 2026-11-02

- Reduced the navigation sidebar default/reset width from 236 to 189 (approximately 20%) and its minimum from 190 to 152. Explicit saved custom widths remain respected; list/reader widths are unchanged.
- Host build succeeded and the previous synthetic preview exited via its guarded tray menu before replacement. VM UI regression passed in 9.49s, including updated drag and geometry expectations.

## Active message contrast · 2026-11-02

- Current message rows now use solid accent blue with white sender, subject, preview, date, unread marker and status icons. Thread badges use a translucent white surface; inactive and multi-selection styles are preserved.
- VM UI regression passed in 9.39s and the synthetic screenshot was inspected. Host build succeeded; the prior preview exited through its guarded tray menu before the replacement opened, preserving one interactive instance.

## Desktop single-instance correction · 2026-11-02

- Root cause: only data-directory locking existed; each interactive demo used a fresh temporary directory and therefore could create another window/tray. Added an independent SingleInstance D-Bus service before storage/controller/tray construction. Repeated interactive starts activate the existing window and exit, regardless of demo/profile arguments. Profile locking remains. Screenshot exports stay isolated with no tray.
- VM process/tray regression passed **2/2 in 9.83s**, covering normal/profile relaunch, demo relaunch, ownership release/restart and screenshot export alongside a running demo. Tests run on a private session bus; CI explicitly installs D-Bus tools. Host build succeeded.
- Removed only the duplicate synthetic host previews started in this chat and replaced the final old preview with the corrected binary. Read-only host process/watcher inspection confirmed **one NkMail process and one registered tray item**. Runtime log was empty. No real mailbox was accessed.

## Remove permanent bottom bar · 2026-11-02

- Removed the entire status strip and extended sidebar, content and column dividers to the window bottom. Theme switching moved to More. Errors, blocked-exit notices and active live-account work use an on-demand floating notice without reserving footer space.
- Host build and synthetic preview launch succeeded. VM UI regression passed in 9.40s; geometry checks now assert full-height panes/dividers and absence of statusStrip. Corrected one stale status.y test reference found on the first run.

## Quiet preview status · 2026-11-02

- Removed the permanent Demo/synthetic-mail status caption and its idle dot from the preview footer. Lifecycle notices and errors remain visible; normal account status behavior is unchanged. Preview mode still uses synthetic data and disables sending.
- Host build and preview launch succeeded; VM UI regression passed in 9.54s.

## Shared sidebar glyph and compose placement · 2026-11-02

- Replaced panelLeft with canonical KosSidebarToggle (rounded outline and filled inset pane, as used by Weather/Calendar), retaining the liquid background. Removed the sidebar-header compose button; main-toolbar compose remains. Recorded the user preference in AGENTS.md.
- Host build succeeded and synthetic preview opened. VM UI regression passed in 9.52s, including pointer activation of the remaining compose action and absence of the removed sidebar button. Initial test lookup of a Repeater delegate returned null; corrected the test to click its stable group coordinates, without bypassing pointer handling.

## Icon-only view toggles · 2026-11-02

- Changed the unread filter and sidebar toggle to compact icon-only liquid buttons. Kept accessible names, hover hints, filter selection state and existing actions.
- Host build succeeded and a new synthetic preview was opened. VM UI regression passed in 9.62s; no host automated tests.

## Grouped inline liquid toolbar · 2026-11-02

- Reduced labels from 12px to 11px and icons from 15px to 14px. Each action group now shares one canonical liquid surface, with transparent interactive cells and subtle short separators; horizontal labels, shortcuts/tooltips and action routing are preserved.
- Host build succeeded and the updated synthetic preview was opened. VM UI regression passed in 9.85s; inspected the 1600px software preview for grouping/layout (not GPU material acceptance).

## Restore inline liquid controls · 2026-11-02

- Restored canonical LiquidActionButton glass material, capsule geometry and theme ink for inline toolbar actions. Each group now receives the shell backdrop and material preference; horizontal icon/label layout remains unchanged. No shared dependency source was changed.
- Host build succeeded and a new synthetic preview was opened. VM UI regression passed in 9.32s. No host automated tests or real-mail access.

## Inline toolbar refinement · 2026-11-02

- Replaced stacked capsule groups with a mail-specific horizontal composition of canonical Kos.Ui buttons: 15px icons, 12px labels, 30px hit targets, hover surfaces and group whitespace. Narrow layouts retain icons and accessible tooltips. Shared dependency sources remain untouched.
- Fixed compact More action routing while preserving action indices and object names. VM UI regression passed in 9.46s; inspected a 1600px software-rendered synthetic preview. Host preview build uses the previously authorized local preview workflow; automated tests remain VM-only.

## System-tray persistence · 2026-11-02

- User explicitly authorized implementing systray persistence, compiling and opening NkMail on the host for this task. Automated regressions remain VM-only; reference projects are untouched.
- Added independent TrayService lifecycle policy and native TrayBackend implementation. Window close saves the draft and hides into the tray; activation restores; menu/Ctrl+Q exits through a draft/send guard. Missing/disappearing tray hosts cannot strand a hidden client. No login autostart was added.
- Default pinned-source Debug builds succeeded on host and VM. VM CTest passed **6/6 in 39.61s**, including new tray lifecycle tests and QML close/send/draft protection regressions. No real account or mailbox writes were used.
- Opened host `build/host-tray/nkmail --demo --appearance=light`; process remained running and its StatusNotifierItem was registered with the desktop watcher. Runtime log was empty. Host automated tests were not run.
- The prior modularity commit fb2749c was published to PR #5; main remains protected and unmerged.

## Modularity implementation · 2026-11-02

- Authorized scope: repair the review findings and split maintainable feature modules. Branch `codex/modular-mail-services`; all builds, execution and tests are restricted to the designated VM. No real account, external SMTP, reference-project modifications, main push or dependency upgrade.
- Added AccountSession, MailboxSync, MailboxCommands, DraftService, SendService and RemoteImageLoader with an injectable MailTransport boundary. Removed duplicated single/batch controller operations; extracted sidebar/list/reader/toolbar/selection/labels QML with explicit inputs/signals and independent loading tests.
- Safety repairs: no whole-mailbox EXPUNGE fallback; UIDPLUS checked before any delete/move write on one session; partial failures are explicit and never automatically retried. Interrupted-send recovery is startup-only. Network pages update active UIDVALIDITY. Confirmed flag updates persist summary and body together. Sidebar reads use scoped SELECTs without rebinding. Background accounts resolve credentials securely; disconnect restore permission survives restart. Draft account ownership is enforced in service and storage. Image cache keys include account identity, store rendered HTML in bounded session memory and support cancellation.
- VM regression milestones: complete five-suite run passed after initial extraction (38.30s); another complete run including disconnect persistence passed (37.96s). New protocol fixture initially lacked mandatory RECENT response; corrected the fixture. Strict QML tests caught a stale search-control ID and incomplete mock interface after toolbar extraction; fixed both without weakening warnings. Final VM Debug/default-source build and all five CTest suites passed in **38.80s** on Ubuntu 26.04 / Qt 6.10.2: UI 20, core 19 and controller 33 QtTest passes (including init/cleanup), plus CLI acceptance and 37 loopback protocol/TLS scenarios. The final command-service input guards and action-time toolbar row lookup are covered.
- VM dependency guard regressions passed **32/32**; VM whitespace/diff check passed. The live dependency guard passed for the unchanged clean canonical gitlink `e2592b2742c58c1bd797a8bcfa2a3ae7da4affd8`, verifying upstream commit existence. This is the inherited commit-only guard, not a published-release acceptance claim. README/NOTICE now accurately identify that existing pin.
- Synthetic light inbox and dark compose captures were generated in the VM and visually inspected (`tmp/modular-review/` locally). The VM Wayland attempt reported an unexposed window; the inspected captures use offscreen/software rendering and are not GPU/material acceptance. No real mailbox/provider verification was performed.
- Ready for review on the local refactoring branch. No remote publication or main merge was performed. Module contracts and regression boundaries are documented in README and docs/TESTING.md.

## Static architecture review · 2026-11-02

- User requested pulling the online NkMail source and reviewing modularity and maintainability. Fast-forwarded local `main` to `fd21cd3da37eea6aa1d6dc0ad4fc1516ef99382e` and initialized the pinned UI submodule. Review scope is merged `origin/main`, not unmerged feature branches. Existing local feature branches were preserved.
- Static inspection only: no compilation, application launch, tests, real-account access or mailbox operations. No implementation fixes, commits or pushes were made in this review.
- High-priority findings: targeted delete/move falls back to whole-mailbox EXPUNGE without UIDPLUS; database `open()` performs interrupted-send recovery even for sidebar probes; background polling receives database accounts whose passwords are explicitly cleared; successful network pages do not update the controller UIDVALIDITY used by batch commands/search. Single/batch flag writes update the model but not persisted cache. Remote-image completion guards outlive the rendered content and omit account identity, preventing subsequent loads after reopening a message or switching to matching IDs in another account.
- Architecture: `MailController.cpp` (1599 lines) mixes account/credential lifecycle, polling, caching, reading, image HTTP, composition, sending and mailbox mutations; `Main.qml` (1658 lines) mixes shell, navigation, selection, toolbar, list and reader. Existing controller tests replace concrete static backend methods at link time; newly added mutation/statistics stubs return empty/success results, leaving these failure paths insufficiently exercised.
- Recommended sequence: fix destructive-operation and send-state issues first with targeted VM regressions; introduce injected mail transport and account-scoped operation context; extract AccountSession, MailboxSync, MailboxCommands, Draft/Send services and RemoteImageLoader with explicit ownership; split sidebar/list/reader/toolbar QML into components with declared properties/signals; separate connection opening, schema migration and one-time interrupted-send recovery. Keep MailController as a thin QML facade. Avoid merely splitting member functions across source files while retaining shared mutable state.
- Documentation drift remains: AGENTS.md/older ledger and account-dialog copy describe read-only/single-account behavior while merged source supports multi-account and server mutations. Reconcile these contracts before release. Runtime acceptance of this revision remains unverified.

## Historical first-release contract

The 2026-11-02 scope and VM-only execution rule above supersede the single-account/read-only scope and local-execution permission below. Historical acceptance results are retained as a recovery record.
Build a working single-account Qt Quick mail client in this directory. Reuse canonical Kos.Ui from the pinned nextkdeAppDemo Git submodule at third_party/nextkdeAppDemo; use mailio for IMAP/SMTP/MIME. External UI workspaces require an explicit NKMAIL_UI_SOURCE_DIR development override. Follow KMusic application patterns and calendar's dialog/popover interactions, with macOS Mail as a visual reference. No server-side flag editing, move or delete in the first release.

The user authorized local builds, GUI and isolated automated testing, installation of the missing Boost development package, and milestone commits/pushes with synthetic screenshots to the public NextKde/NkMail repository. A specific enterprise test account was subsequently supplied for local read-only provider verification, after TLS/PEEK acceptance. Its details and credentials must never enter this repository or public output. No existing mail-client access, external sending, mailbox mutation, default-client registration or reference-project modification.

This file is a recoverable work ledger, not a native Codex Goal. Work requires an active session. The session-only progress timer cannot run after the session exits.

## Acceptance checklist
- [x] Independent CMake build with canonical Kos.Ui and pinned mailio.
- [x] Reviewed TLS verification and side-effect-free PEEK patch, with negative tests.
- [x] Actual loopback IMAP/SMTP integration and MIME/attachment tests.
- [ ] Single account, secure credentials, folders, paginated summaries and on-demand content: implemented; controller/provider integration still to validate.
- [x] Offline cache and cached-only search; UIDVALIDITY-safe identity (core regression).
- [x] Compose/reply/reply-all/forward, attachment copy/save/discard and restart-safe drafts: actual controller integration regression. Unsupported complex recipient formats fail closed for automatic replies; manual composition remains available.
- [x] Send accepted/rejected/ambiguous states, no automatic retransmission (protocol and persistence regression).
- [x] Three-pane UI, dark/light/narrow layouts, canonical dialogs/popovers and keyboard use: rendered, behavior-tested, resizable columns and per-profile geometry persistence; actual two-start window restore checked.
- [x] Automated behavior regression plus real GPU screenshots using synthetic data.
- [x] Public milestone reports and six synthetic screenshots published to main after staged privacy checks.

## Verified facts · 2026-10-29
- Public repository cloned; main created locally; GitHub access and commit identity available.
- Authorized Boost development installation completed successfully.
- mailio pinned to 0.26.1, commit `f358b1f85f9a021f0bd814e285eb8ae102fc8f79`; complete local security/read-only patch retained in patches/mailio/ and reverse-apply checked against vendor source.
- Kos.Ui sibling source HEAD: `54fd5f14a5bc50132d524d053b373b1dc1189935`, with existing local component changes. Those changes are consumed as-is, not altered or copied; HEAD alone does not reproduce this workspace.
- Actual application and test builds succeeded on Qt 6.12.0. Final integrated CTest: **3/3 passed**, 20.46 seconds; core QtTest 16 pass, UI QuickTest 9 pass (including init/cleanup), protocol fixture 30 scenarios.
- Actual GPU app screenshots checked: inbox light/dark, compose, account, details, narrow. All synthetic, sending disabled. Curated artifacts in docs/progress/.
- Fixed initial read-only TextEdit implicitHeight QML error and preview theme initialization. Final UI tests have no QML warnings; upstream C++ and CMake tooling warnings remain.
- Draft state transitions use authoritative database CAS, interrupted sending becomes unknown on restart, and data-directory QLockFile prevents concurrent app instances from reinterpreting a live send.
- First public milestone successfully pushed: `4e4dfea7b51690e01b7ecf8be3c823c0b079529b`, including six reviewed synthetic screenshots.
- A single controlled read-only provider attempt failed before the header/content slice completed; no alternate password was tried. A separate no-login connection verified system CA/hostname TLS and an IMAP greeting, but fixed backend diagnostics do not identify the failed authenticated phase, so this is not proof of an authentication failure. Provider compatibility remains unverified. No real SMTP submission or mailbox mutation has occurred.

## Historical remaining work / known limits (superseded)

This section records the 2026-10-29 baseline, not current missing features. Multiaccount, Sent APPEND, flags/move/delete and legacy Google OAuth were subsequently implemented. See the current queue below.
- Controlled provider read-only verification after the now-passing TLS/PEEK suite; never publish account data or raw output, never cycle password alternatives automatically.
- Controller-level workflow regression added: 23 passed including init/cleanup; reproduced and fixed offline paging, UIDVALIDITY append reset, stale attachments, visible cache on transport change, profile keychain isolation and reply recipient handling.
- Column resizing, window geometry persistence and keyboard/focus checks implemented; pointer-test scene-coordinate issue fixed with unchanged strict assertions, three consecutive UI runs passed.
- Additional backend-only defensive review was interrupted before handback; it is not a completed audit or acceptance result.
- No server Sent APPEND; no remote flags/move/delete/OAuth/IDLE/multiaccount.
- Folder statistics currently use sequential STATUS; network deadlines are per-I/O, not whole-operation limits. MIME tree bounds are checked after mailio parsing, not a complete upstream parser audit.

## Required UI integration gate · 2026-10-31

- Added a read-only release/dependency guard with an explicit tag/SHA lock and 28 offline negative/positive regression tests (all passed locally), including hidden assume-unchanged/skip-worktree index flags. Live verification of the existing canonical prerelease passed.
- [PR #1](https://github.com/NextKde/NkMail/pull/1) bootstraps the hosted gate. [Initial hosted run](https://github.com/NextKde/NkMail/actions/runs/41173564341) passed the first 26 guard tests, live release check, fresh default-source Release build and **5/5 CTest suites in 28.36 seconds**, using Qt 6.12.0 / OpenSSL 3.6.5. The updated 28-test run is in progress; PR has not yet merged.
- GitHub main protection was enabled and read back: PR required, zero outside approvals, enforce_admins=true, strict required `release-gate` from GitHub Actions app 15368, conversation resolution required, force-push/deletion forbidden. The first protected merge and main push run remain to be verified.
- Existing `v0.1.0-rc.1` remains unchanged; this task does not modify or publish upstream components. The guard complements fixed versions and cannot infer untested changes in another developer workspace.

## UI dependency release pin · 2026-10-31

- Replaced the default sibling workspace with a canonical Git submodule at `third_party/nextkdeAppDemo`, fixed to source prerelease [`v0.1.0-rc.1`](https://github.com/NextKde/nextkdeAppDemo/releases/tag/v0.1.0-rc.1), commit `dd0d5f3e7a7eab75c7bd465f29709d5f86e8091d`. The annotated tag and GitHub prerelease are published; this is not a stable/binary SDK release.
- Fresh default-source Release configure/build succeeded on Qt 6.12.0 / GCC 16.2.1. Existing CTest suites passed **5/5 in 32.82 seconds** (CLI, software/offscreen UI, core, controller, loopback protocol); no real mailbox was accessed. A separate clean recursive clone of integration commit `2a697b1d48fc632ecc8191be25ec7e432c765171` initialized the UI source from GitHub and independently built/passed **5/5 in 29.89 seconds**. Both runs used the exact commit now tagged for the release.
- Verified an uninitialized clean source snapshot fails with the documented submodule-init command, the fresh default cache points inside this checkout's submodule, both consumer copies have the exact clean candidate SHA, and an explicit sibling development override still configures. Existing Qt/QML tooling/plugin discovery and upstream mailio compiler warnings remain; no new qmllint, standalone Demo or GPU acceptance is claimed.
- CMake retains an explicit `NKMAIL_UI_SOURCE_DIR` development override. Default builds neither follow a moving branch nor fall back to the sibling workspace. Existing caches must be reset or replaced to verify the pin.
- Historical 2026-10-29 source/acceptance facts below remain historical; they did not use this release pin. The 2026-10-30 skeleton record is likewise a prior iteration, subsequently committed as `d47833d`, not the current working-tree state.

## Prior UI iteration · 2026-10-30

The user rejected the previous shell presentation. The current uncommitted iteration is **skeleton only**, not visual acceptance or a published milestone:

- Replaced the empty 30px chrome band plus pane-local toolbars with one real 68px header owning sidebar/list/reader actions and search. Sidebar body and list/reader content now start at y=68 in separate shell containers.
- Test loading now uses the compiled TestViews module so MailStyle singleton dimensions resolve. Strict header/body geometry, control ownership, pointer input, compact details anchoring and eight width boundaries are tested; warnings fail UI tests.
- Local build succeeded. Direct software/offscreen UI run: **18 passed, 0 failed, no QML warnings** (including init/cleanup). Full isolated CTest: **5/5 passed, 29.75 seconds**.
- Actual Wayland/GPU synthetic screenshots inspected: wide light/dark and 580px list/reader, in `tmp/skeleton-review/`. No black frame or header/control overlap observed in those four captures. This is not user approval.
- Existing demo PID 120340 was left running; it still shows the earlier layout. Capture processes exited after saving. No second persistent demo was started, no real mailbox contacted, no commit or push performed.
- Next: user inspection of the structural layout before any detail work. Provider compatibility remains unverified. qmllint was not validated in this iteration; runtime loading and tests are the checks reported here.

## Published milestone (prior to the current UI review)
First runnable milestone published. Second workflow/window milestone published as `d5d5b7d46cfa3a6435a283b0b1b3c505d747abbe`, including two further reviewed synthetic screenshots. Final committed code verified in **Debug: 5/5 CTest passed, 27.96 seconds** and **Release: 5/5 passed, 27.92 seconds**. Coverage: core 16 / controller 23 / UI 14 pass (Qt tests include init/cleanup), CLI 3 process cases, protocol 32 loopback scenarios. Working tree was clean after publication; no real mailbox data or credentials were included.

The scoped local implementation is substantially complete, but real-provider read-only verification is blocked at an unidentified authenticated phase. Do not report the whole first release or real-account compatibility complete. Next useful work is credential-free phase diagnostics and controlled resolution with account-owner input, not repeated password attempts or arbitrary UI expansion.

## Sidebar heading review · 2026-11-02

- Reduced the sidebar Mail heading from 24px Bold to 15px DemiBold; aligned it with the existing top-row controls and reserved space for the shared sidebar toggle. No dependency or mail behavior changes.
- Fresh default pinned-source VM build succeeded. VM CTest `nkmail_ui`, `nkmail_i18n`, and `nkmail_translations`: 3/3 passed. Actual synthetic X11/GPU screenshots inspected at 1240px light/Chinese and dark/English, plus 900px Chinese. No heading/control overlap observed. Nested Wayland screenshot attempts returned “Preview window is not exposed”; Wayland visual acceptance is not claimed. No real mailbox was accessed.
- Rebuilt the existing host `build-clean/nkmail` entry without launching it or running tests. Its existing sibling Kos.Ui development override was preserved; default-source validation refers to the separate VM build. No commit, publication or dependency upgrade performed.

## Empty inbox review · 2026-11-03

- Fixed the header fallback to resolve `INBOX` through the existing localized mailbox labels when the folder list is empty. Custom folder names remain unchanged.
- Kept account creation in the sidebar and removed duplicate account buttons and unused signals from the message list and reader. The list's cached-unread reset action remains available. Removed obsolete translation entries from both catalogs.
- Added a UI regression for the empty-account folder fallback, sidebar add-account dialog opening, absence of the list add-account action, and custom folder naming. VM default pinned-source build succeeded; UI/i18n/catalog checks passed 3/3 in 38.55 seconds (48 UI cases including init/cleanup). No host build, tests or GUI execution performed.

## Inbox account entry follow-up · 2026-11-03

- User requested an additional account entry in the inbox empty state. Added a canonical KosButton with the neutral secondary solid style (30px, no liquid rendering or raised surface). Sidebar entry remains; reader duplicate remains removed. Both entries open addAccount().
- VM build passed. Final UI regression passed 48 cases including init/cleanup in 37.65s; i18n and catalog checks passed (0.04s / 0.10s). The first UI attempt checked visibility before the empty list updated; now the regression waits for the list count and button visibility before clicking.
- At the user's explicit request, rebuilt and replaced the host's normal Chinese-mode NkMail process without modifying account/mail data. Automated tests ran only in the VM.

## Reader layout correction · 2026-11-03

- Corrected table fallback separators, blocked-image broken icons, empty layout blocks and source whitespace in the rich-text reader; old cached presentation is normalized without rewriting mail data. Inline pipes/preformatted text are preserved. Header mailbox labels are translated. Full browser/CSS fidelity remains out of scope for this Qt rich-text correction.
- VM final UI/core checks passed 2/2 in 39.05s (49 UI / 22 core cases including init/cleanup). i18n/catalog and loopback protocol passed. Synthetic software capture inspected; see docs/progress/2026-11-03-reader-layout/. The initial UI assertion compared serialized rich HTML and was replaced with rendered-text/geometry assertions.
- Host rebuilt and normal Chinese-mode process replaced for requested manual inspection. No host automated tests. No real email or screenshot published.

## Browser mail layout · 2026-11-03

- Replaced simplified rich-text display for newly fetched HTML mail with an isolated Qt WebEngine Quick reader, preserving passive CSS/table layout in a separate `displayHtml` field. Simplified HTML remains separate for quoting and offline fallback. Old selected cached bodies upgrade once using existing IMAP PEEK, with an account-reconnection action for offline profiles.
- Browser profiles are off-the-record, cookie-free, sandboxed and network-blocked. CSP/page settings disallow active content and local access; inline images arrive only through the opt-in loader. Current-only opaque memory documents avoid encoded loadHtml size limits. Application-owned isolated-world code measures height.
- Fixed plain/rich fallback switching and synchronous width/text binding hazards found by strict regression checks. Final VM browser/UI checks passed 2/2 in 42.35s (6 browser / 50 UI cases including init/cleanup). Core/controller/CLI/tray/i18n/catalog/protocol checks passed during staged regression; controller 37 cases. Reviewed synthetic integrated screenshot at docs/progress/2026-11-03-browser-reader/.
- No host automated tests; host build/relaunch for requested manual inspection. Qt compatibility-profile deprecation and expected attack-fixture CSP diagnostics remain. No Chromium sandbox bypass, real mail publication or remote mailbox mutation. Hosted gate pending on the updated PR.

## Gmail setup diagnosis and screenshot publication · 2026-11-04

- Browser-reader commit 5dc43594f72f16713a98681e5583426c25cca7fa passed hosted release-gate (run 41640362986). The PR description now embeds the reviewed synthetic browser-reader screenshot directly. No real account screenshot or credentials are published.
- Clarified generic password labels and provider-specific Gmail guidance: NkMail currently uses IMAP/password authentication, supports Google app passwords where allowed, and has no Google OAuth or POP implementation. The ordinary Google password path is not supported by Google's current policy. Updated English/Chinese catalogs and docs/PROVIDERS.md with official sources.
- VM no-auth connectivity probes for imap.gmail.com:993 and pop.gmail.com:995 timed out in TCP connect. No TLS/authentication attempt or mailbox action occurred, and no account-password validity conclusion is supported by this probe. VM connectivity is not proof of the host's exact failure.
- VM UI/i18n/catalog checks passed 3/3 in 40.18s. Host guidance build succeeded; normal-mode relaunch for manual use. Hosted check for the new guidance commit remains separate from the passed browser-reader check.


## Project-managed Google OAuth · 2026-11-04

- Implemented system-browser authorization with PKCE S256, one-use validated IPv4 loopback callbacks, verified email identity, cancellation/timeouts, fixed Google endpoints, offline refresh and rotation through QtKeychain. No tokens in account metadata/QML/SQLite, no per-user client configuration, no password fallback for Google OAuth accounts.
- Project Desktop OAuth client is created and stored outside the repository; Gmail API enabled, required scopes declared, test user saved by owner. The project remains Testing. Production branding/privacy/domain verification and applicable Google restricted-scope review remain outstanding; public availability is not claimed.
- Added mailio IMAP/SMTP XOAUTH2 with SASL-IR/continuations/error acknowledgment and no real sending. Account setup saves verified Google identities independently of mail-port reachability; failed persistence can retry without new consent. Standard-password providers retain auth-only IMAP/SMTP checks.
- VM pinned-source Release build with bundled synthetic client configuration passed 10/10 CTest suites in 86.23s. New OAuth fixtures cover callback/state/PKCE/identity/refresh/concurrency/rotation/cancellation/redirects; controller tests assert keychain persistence without token serialization. UI save retry and one-click login pass. Reader timing regression fixed by waiting for complete Chromium parsing, retaining its 300,000-character assertion.
- Curated synthetic screenshot: docs/progress/2026-11-04-google-oauth/sign-in.png. Host configured build launched for the user's real manual Google consent; automated tests ran only in VM. User reported that real account addition failed; the reported message combined missing mail scope and missing offline access, and the user sees two previously granted permissions without a checkbox. The actual missing part is not yet established. Split those error branches and added consent guidance without assuming a checkbox is shown. Real acceptance remains unresolved. Code and synthetic screenshot are pushed as 3c7d70a in PR #18; hosted release-gate for 3c7d70a passed (run 41752702276). No real mail writes, password use or public credentials.

- Consent follow-up: supplied permission descriptions include full Gmail mail access, so a missing-checkbox explanation is not established. A later attempt reports secure-connection failure. Added credential-free request diagnostics limited to fixed stage, network enum, HTTP status and allowlisted provider error code; no URLs, responses, headers or token values are logged. Diagnostic build is running for manual retry. VM UI/i18n/catalog/OAuth checks passed 4/4 in 43.08s; the final diagnostic OAuth rebuild/check passed again. Real acceptance remains unresolved.

- Latest real retry: credential-free journal diagnostic reports authorization-code exchange closed by remote peer before any HTTP reply (Qt network enum 2, HTTP status 0). This establishes a transport failure for that attempt, not an OAuth policy denial. No token or response body was read into diagnostic output. Browser/application routing differences are being checked; no system network settings were changed. Awaiting the user's choice before trying an existing local proxy for this process.

- User authorized a temporary process-only proxy. The next live attempt passed token exchange but the additional UserInfo connection closed before an HTTP response. Replaced the unnecessary UserInfo request with private identity extraction from Google's TLS-authenticated token response, following OIDC Core 3.1.3.7 step 6 / Google server-flow guidance. This does not accept externally supplied JWTs or serve as a general signature verifier. Validate RS256 structure, Google issuer, audience/azp, fresh per-flow nonce, subject, expiry/issued-at, verified email and optional access-token hash. ID token is discarded, never persisted or exposed. VM OAuth/controller checks passed 2/2 in 7.68s; final pinned-source Release suite passed 10/10 in 86.50s. Host updated for the authorized manual retry; system settings remain unchanged.


## Real Google sign-in accepted · 2026-11-04

- User confirmed that the configured build successfully added the real Google account after the backchannel identity change, using the explicitly authorized temporary process proxy. This verifies browser authorization, token exchange, identity acceptance and account addition. It does not establish mailbox synchronization, restart/refresh-token restoration, SMTP submission or delivery. No real mail write was performed.
- Implementation and synthetic screenshots are pushed in PR #18; code commit 2e82bc5 passed final VM Release 10/10 suites in 86.50s. Hosted release-gate for this code remains pending at the time of this record (run 41755614217). Google project remains Testing; public branding/privacy/domain and applicable verification work are still required for general distribution. System network settings were not changed.


## Google mail-list wait · 2026-11-04

- User reports indefinite “Getting your mail” after successful account addition. Read-only cache diagnostics established that Google folders were saved but no message page was cached; a momentary socket snapshot alone does not establish the full cause.
- Replaced up to 50 sequential header requests with one bounded UID FETCH. Preserve newest-first search UID order, tolerate vanished messages and keep sequence-window pagination; retain EXAMINE/BODY.PEEK and literal limits. Loading UI now shows the current localized phase. Opt-in diagnostics output only fixed stage names and elapsed milliseconds, never wire data or private identities.
- VM build and UI/i18n/catalog/protocol checks passed 4/4 in 66.11s; controller checks passed separately. Sparse/out-of-order/vanished-header cases exercise TLS and STARTTLS, assert a single bounded request, and prohibit writes. Synthetic phase screenshot: docs/progress/2026-11-04-mail-sync/loading.png. Host rebuilt/relaunched with existing authorized process-only proxy and sync diagnostics. User confirmed the Google mail list appeared. Fixed-stage diagnostics recorded authenticated/select/search/header-fetch completion in approximately 7.9s for this page. This accepts live header-list retrieval, not body rendering, background token refresh, restart restoration or sending.


## Message body wait and empty header · 2026-11-04

- User reports slow Google body opening, no visible loading and a question-mark/no-subject/empty-recipient header. Confirmed that openMessage cleared current presentation before asynchronous content completed, even though the selected list already had header metadata. This UI bug is independent of any network latency.
- Present list summary immediately; use transient bodyPending/bodyLoading/bodyError fields, visible download/render indicators and local Retry. Hide unknown headers/missing recipients. Preserve cached bodies during legacy-format upgrades; block reply/forward while a new body is pending. Generation checks and clearing previous attachments remain in place.
- Added credential/content-free phase timing around content authentication, folder selection, fetch/parse and preparation. VM browser/protocol/i18n/catalog passed; corrected UI/controller fixture setups, then final UI/controller checks passed 2/2 in 49.14s. Reviewed synthetic capture at docs/progress/2026-11-04-body-loading/loading.png. Host updated with existing authorized process proxy and diagnostics; user asked to open an uncached Google message. Real body timing and visual result remain pending; no network-cause claim yet.


## Follow-up body timing and QQ credential state · 2026-11-04

- User confirms the body loading indicator is visible but opening remains slow. Fixed-stage live diagnostics show two content operations completing in ~11.73s and ~3.48s. Authentication/connection took ~7.09s/~2.40s; fetch/protocol parsing ended at ~11.72s/~3.47s, with subsequent local preparation ~8ms/~6ms. This identifies connection/protocol waits as the dominant measured cost, not an isolated renderer benchmark. Each uncached body currently creates a fresh IMAP connection; connection reuse remains an optimization opportunity. No raw diagnostics or mail data are published.
- QQ's “application password before changing messages” prompt is a local missing-credential guard, not evidence of server rejection. A session-only credential cannot survive application restart while restore permission is disabled. Clarified missing-credential prompts for QQ authorization codes and Google sign-in, preserving restore permissions and the no-credential write guard. User chose to handle QQ re-entry/keychain remembering later; do not change that permission or request the code in chat.
- VM i18n/catalog checks passed; final controller rerun passed in 5.87s after correcting the new fixture to populate the QQ-bound cache after its account identity change. Host compilation is allowed, but the current process is kept running so this copy-only prompt update does not discard session credentials. Real QQ credential restoration is deferred by the user; no QQ password validity claim or real mailbox write.


## Actual loading animation · 2026-11-04

- User reports several loading indicators remain static. The list used a literal ellipsis; other states used native Basic indicators or text-only feedback. Introduced an app-local explicit scene-item activity animation without modifying the pinned component library. Reader, list, search, sidebar, account setup and sending use it.
- Normal motion rotates; reduced motion breathes opacity slowly. Hidden/stopped feedback ceases animation. Tests now verify actual different pixels over time, not merely running=true; normal/reduced motion, hide/stop/resume and integrated body/list views pass on both software and Xvfb/OpenGL. Direct clipped-subitem captures were found unsuitable for the integration assertion, so final tests capture the enclosing views. Do not claim that those initial subitem grabs isolate a Qt rendering defect.
- VM full reader/UI/i18n/catalog suite passed 4/4 in 49.40s; OpenGL targeted animation checks and software targeted checks passed. Reviewed 12 separately captured synthetic frames and assembled docs/progress/2026-11-04-animated-loading/loading.gif; every neighboring frame differs. Host rebuilt/relaunched with the previously authorized temporary process proxy, no host automated checks. QQ credential recovery remains deferred by the user; no restoration settings were changed.


## Reader scrollbar and remote image retry · 2026-11-04

- User reports no body scrollbar and remote images still absent after clicking Load images. Added explicit scrollbar sizing/anchoring and overflow policy, bound content height, and routed embedded-browser wheel events to the reader for normally measured documents. Retain internal browser scrolling for the existing 12,000px capped surfaces, while avoiding redundant native bars for normal documents; height measurement now includes the document element.
- Remote image work is four-way concurrent with existing 20-URL/2MiB limits, a 16MP image bound and 30s active-batch deadline. Show actual animated pending state and disable duplicate clicks. Preserve failed sanitized URLs with a failure marker so Retry can succeed; successful image display styles remain intact. Cookie/Referer-free requests, TLS verification and opt-in behavior remain unchanged. Fixed image diagnostics expose only network enum/HTTP status.
- VM reader/UI/controller/i18n/catalog passed 5/5 in 58.17s; final anchoring/measurement reader and full UI follow-ups are recorded separately. Tests prove visible scrollbar geometry, wheel movement, actual thumb dragging, four concurrent fixture requests, failure preservation and retry without re-fetching successful images. Reviewed synthetic capture at docs/progress/2026-11-04-scroll-images/reader.png.
- User explicitly confirmed scrolling and images both work in the updated host run. The earlier real image failure's precise network/server cause was not captured; do not claim one. Host uses the previously authorized process-only proxy, no global network or image privacy settings changed. No host automated tests or real mailbox writes.


## Gmail app passwords instead of browser sign-in · 2026-11-04

- User chose to hide OAuth onboarding rather than manage public Google OAuth verification. Removed account-dialog browser sign-in state/actions/hints and switched Gmail onboarding to the ordinary email/app-password path. Provider instructions require two-step verification and distinguish an app password from the main Google password. No Google Cloud setup is required for this path.
- Retained the existing OAuth backend and optional project client configuration for compatibility with accounts already added. No token/account deletion, automatic credential conversion, console mutation or access revocation. Editing an existing OAuth account accepts a verified app password under the same ID; regression fixtures prove cached body retention. Missing/expired legacy authorization guidance directs users to account settings.
- VM build and UI/controller/i18n/catalog checks passed 4/4 in 53.80s after the final UI/migration changes. Reviewed actual synthetic screenshot docs/progress/2026-11-04-google-app-password/setup.png; demo disables fields/saving, so it is visual evidence only. No host launch/tests and no real mailbox writes or password submissions for this iteration. The currently running host binary has not been replaced.
- Final backend prompt build and controller/i18n/catalog follow-up passed 3/3 in 6.23s. Hosted release-gate remains the merge requirement.


## Ordinary reader actions and account hierarchy · 2026-11-04

- Replaced the remote-image load/retry LiquidActionButton adapter with canonical ordinary KosButton, using its neutral secondary material. Body-download Retry also uses neutral secondary material. Actions and pending-state disabling remain intact.
- Indented every account folder row by 18px relative to its account header, including inactive accounts; favorites and smart mailboxes retain their existing alignment.
- VM Release build passed; UI/i18n/catalog checks passed 3/3 in 47.43s. Updated the existing remote-image behavior assertion for the ordinary button text. Optional VM screenshot attempt did not run because xvfb-run was absent from the default PATH; no new visual-capture claim. No host build/launch/tests or real mailbox writes. Running host binary remains unchanged.


## Scrolling, account disclosure and favorites · 2026-11-04

- Mouse wheel scrolling in message lists and ordinary-height reader content now uses a 160ms easing transition; touchpad pixel deltas remain direct. Capped WebEngine documents retain their internal scrolling. Shared handler respects reduced motion and interrupts animation for dragging.
- Account headers only expand/collapse. Selecting a child folder performs account navigation. Added a UI regression for a non-current account header preserving current account identity.
- Custom favorites have a remove context action. Successful account removal clears corresponding persisted favorites; older orphan entries are filtered from display.
- VM controller/i18n/catalog passed; initial reader/UI load found a handler object-property error, fixed before delivery. Final reader/UI passed 2/2 in 52.66s. Host compiled and relaunched under the user's existing authorization, preserving process environment; no host automated tests or real mailbox mutation. Perceived scrolling smoothness awaits user feedback.


## Reader surface on window restore · 2026-11-04

- User reports a black HTML body after minimizing/restoring, recovering on scrolling. This suggests a stale rendered surface; the host-specific GPU/compositor cause has not been isolated.
- Bind WebEngine item visibility to its containing window's minimized/hidden state, explicitly notifying the browser renderer through suspend/resume without replacing the document or resetting scrolling.
- VM reader suite passed in 2.89s, including new minimize/restore and hide/show pixel checks without user input, with document URL/readiness preserved. This VM result does not establish reproduction or resolution on the user's compositor.
- Host rebuilt/relaunched with prior process environment under existing authorization. No host automated tests or mailbox mutations. Await user confirmation of the original minimize/restore symptom.


## Restore follow-up: visibility synchronization insufficient · 2026-11-04

- User reports the black surface still occurs. Prior VM success did not reproduce or resolve the host symptom. Host session is Wayland; no explicit Qt renderer override was present. No definitive GPU/compositor cause established.
- Replace ineffective view-visibility workaround with a debounced local-document reload on restore/reactivation. Reload uses the existing isolated in-memory URL, not mail transport; preserve document dimensions, outer reader position and internal browser scrolling. New mail cancels pending restoration. This is a rendering recovery workaround; browser text selection can reset on reactivation.
- VM reader suite passed in 4.08s, with pixels checked after queued restoration and a long-document regression confirming internal scroll restoration. Host rebuilt/restarted with original process environment. No host automated tests, mailbox writes, global renderer changes or credential changes. Actual host symptom confirmation remains pending.


## Cover transient restore frames · 2026-11-04

- User confirmed local-document recovery fixes the persistent black body, but black content remains during the minimize/restore animation.
- Cover the embedded renderer with its normal white document background while suspended/recovering. Keep rendering active underneath; remove the cover 120ms after load completion when the window is no longer suspended. This hides transient black content rather than preserving a screenshot of mail; a brief plain background may be visible during recovery. Native compositor animation behavior still requires user confirmation.
- VM reader checks passed in 4.12s, including pixel checks for the cover, cover removal, restored content, and retained internal scrolling. Host rebuilt/restarted with original process environment. No host automated tests or mailbox mutation.


## Retained reader surface: final compatibility adapter · 2026-11-04

- User confirmed the targeted texture-lifetime intervention resolves the native Wayland minimize/restore black region. Passive lifecycle evidence showed render-loop suspension without scene-graph invalidation. An initial QObject-tree probe never attached and is not valid evidence; the corrected visual-tree probe confirmed attachment before the successful comparison.
- Removed all focus-reload, visibility/cover workarounds and host-only diagnostic instrumentation. MailRenderLifecycle observes the reader's visual children/window changes through queued event-driven scans. It preserves the delegate's front-buffer resources on suspension while retaining actual invalidation/destruction cleanup. No polling or global Qt modification.
- The adapter deliberately depends on Qt's private render delegate identity using public QObject APIs. Regression requires successful attachment, no document reload/scroll loss, and fresh attachment after reparenting. Revisit this compatibility adapter on Qt upgrades. No upstream issue was submitted.
- VM full suite passed 10/10 in 95.52s. Isolated KWin Wayland reader suite passed 8/8; graphics diagnostics during image capture mean this is not artifact-free animation acceptance. Final attachment-state/reparent regression passed in 3.97s. Earlier capture-only assertions were insufficient to prove passive animation correctness.
- No host automated tests or mailbox mutation. Diagnostic comparison was user-confirmed; final event-driven adapter uses the same scoped disconnection and is prepared for host manual acceptance.


## Current delivery queue · 2026-11-04

1. Event-driven retained-surface adapter accepted by the user; PR #18 merged after release-gate.
2. Provider/testing documentation reconciled. PR #19 includes IMAP session reuse and the body/flag race fix; merge only after its required release-gate passes.
3. IMAP header/body session reuse implemented on codex/imap-body-session; VM regressions pass. Real-provider latency and final PR release-gate remain to verify.
4. Cross-account unified inbox and read-only aggregates with explicit account/folder/UIDVALIDITY/UID routing.
5. Favorite ordering and unknown/stale unread-count semantics; complete copy-to/junk workflows and message/rfc822 attachment forwarding.
6. CID embedded images; controlled provider/app-password acceptance. QQ credential restoration remains user-deferred.
7. Deferred product scope: IDLE/background service, Microsoft/Exchange, colored flags/reminders/muting/blocking/rules. Public Google OAuth onboarding/review remains deliberately deferred, not an app-password release blocker.

Old unchecked lists and provider statements above are historical evidence, not an additional duplicate backlog.


## Reuse short-lived IMAP connections for uncached bodies · 2026-11-04

- Header pages and uncached bodies opt into a worker-thread-local connection on the existing dedicated serial MailboxSync queue. A recent header page can warm the first body read; successive bodies reuse authentication. No prefetch or write-session pooling.
- Identity digest covers account/user/endpoint/authentication/TLS/custom trust anchor and credentials. Reuse expires after 60s; the owner queues idle cleanup and explicit cleanup on switch, settings save, removal and disconnect. All cleanup stays on the worker; worker teardown closes its thread-local session.
- Every operation still uses read-only EXAMINE; bodies check UIDVALIDITY before UID BODY.PEEK. Transport failure on a reused connection gets at most one fresh attempt. Fresh-connection failure, protocol errors, UIDVALIDITY changes and TLS rejection do not loop. Send/write retry semantics are unchanged.
- VM full suite passed 10/10 in 98.24s. Final protocol follow-up (actual server UIDVALIDITY change and controlled timing) passed in 25.79s. Fixtures cover header/body reuse, two UIDs, cross-folder reading, account/password/trust changes, explicit reset, closed connection, bounded double failure, protocol NO and controller cleanup.
- With an artificial 150ms login delay, fixture reads measured 214→4ms (implicit TLS) and 168→2ms (STARTTLS). This proves avoided handshake/authentication work, not real-provider end-to-end latency. Cold/expired sessions and body transfer/parse still take time. No real mailbox writes or automated host validation.

## Uncached body and flag-update race · 2026-11-04

- Opening an unread row starts a body read and then marks it read. Flag-command dispatch invalidated both the list and body generations; completion invalidated the body again and rebuilt selection from header-only content, removing the loading state. The otherwise successful body callback was discarded.
- Body delivery now depends on the selection generation, independent of list refresh/filter/flag changes. Account/folder/selection changes and completed removal still invalidate it. Flag completion updates selection flags in place, preserving pending/error/body state; a late PEEK snapshot preserves current read/star flags before caching.
- Four deterministic VM regressions reproduced the old bug: read/star updates completing before/after the body. All four failed before the fix. Tests also cover large HTML delivery through callback/cache/selection. After the fix, the full VM suite passed 10/10 in 100.16s, including all four race cases.
- Temporary MIME/DOM/dispatch diagnostics and the diagnostic empty-cache bypass have been removed. No real-mail content or raw logs are published. Host builds/replacements are for user manual inspection; automated tests run only in the VM.

## Post-fix diagnostic cleanup · 2026-11-04

- User confirmed the uncached-body blank issue is resolved. Removed the remaining opt-in IMAP phase timing and image-request diagnostic output, their logging includes and a duplicate include. Lifecycle fixes, error handling and race regression coverage remain intact.
- Remote images currently require per-message consent and use an 8 MiB in-memory rendered-body cache. Persistent per-message consent and sender exceptions are not implemented; no automatic-loading policy was changed in this cleanup.
- Cleanup validation: VM controller and loopback protocol tests passed 2/2 in 32.77s; host preview rebuilt and replaced for manual use.

## Persistent remote image permissions · 2026-11-04

- Added persistent per-message permission, account-scoped sender allow rules and an opt-in global auto-load switch. Defaults remain blocked. Display options can revoke the current sender or reset all permissions; account removal cleans up its rules. Policy storage stays in RemoteImageLoader, with hashed account/message/sender keys.
- Auto-loading happens only for the currently presented message and reuses the bounded cookie-free loader. Per-message consent includes folder and UIDVALIDITY, preventing UID reuse across folders/epochs from inheriting permission. Image bytes remain in-memory rather than becoming a persistent disk cache.
- Added policy persistence/isolation/revocation tests, synthetic loopback controller coverage for restart and sender/global loading, and UI action/settings/width checks. Tests and screenshot run in the VM only. Controller/protocol/core/CLI checks passed in the initial suite; after removing the obsolete translation and waiting for QML layout in the new geometry test, reader/i18n/catalog checks passed and the final complete UI suite passed in 49.81s.

- Image action polish: switched reader actions to the existing compact MailAction adapter (14 px image/sender icons, 28 px rounded material buttons); renamed the sender option to “允许此发件人显示图片” / “Allow images from this sender” in both the reader and display settings. Display options remain in the right toolbar or More → Display on narrow windows.
- VM reader/UI/i18n/catalog checks passed 4/4; final icon-and-label width/interaction checks passed, and the updated narrow-reader screenshot was reviewed.

## CID embedded message images · 2026-11-04

- Resolve case-insensitive cid: schemes and percent-encoded IDs against current-message MIME parts, normalizing mailio's non-strict angle-bracket IDs. Duplicate IDs are ambiguous and do not select an arbitrary part.
- Validate decoded raster type/size/pixels before creating application-owned data URLs; raw sender data/file URLs and SVG remain blocked. Missing, invalid and budget-exceeding resources use escaped alt text. Inline images stay out of the attachment list unless explicitly attached.
- Cache format 2 stores resolved image HTML; version 0/1 messages upgrade only when selected and connected, preserving their fallback during the read. Existing remote permissions remain independent.
- VM core/controller/UI/i18n/catalog checks passed in the full run. CID protocol fixtures passed over TLS and STARTTLS after correcting an invalid synthetic PNG and normalizing Content-ID brackets. Reader checks passed in 9.76s; reviewed the synthetic embedded-image capture. No host automated validation or real mailbox writes.

## Cached cross-account unified inbox · 2026-11-04

- Added a clearly labeled cached, read-only inbox aggregate. Source account is part of list identity/thread grouping and shown in each row; SQL joins only saved accounts and current folder epochs. Cross-account cache writes are rejected.
- Opening a row switches credential/body/reply context while preserving the unified view and loaded pages. Pending uncached bodies resume after credential restoration. Aggregate views block flag/move/delete commands in the controller and UI; the row menu opens the original account folder.
- First VM run exposed a missing account argument in incremental list replacement: equal UIDs matched another account's row and attempted an invalid move. Fixed the lookup and added duplicate-identity reorder coverage, plus controller pagination and UI routing checks. Final VM suite passed 10/10 in 105.87s, including UI, core, controller, CID protocol and reader checks. The canonical UI pin gate and 32 dependency guards also passed. Synthetic screenshot reviewed; host compiled for manual use only.
- Still pending: remote synchronization across all accounts, copy-to/junk workflows, message/rfc822 forwarding, persistent image-byte cache, favorite ordering/stale counts, and the explicitly deferred provider/background-service scope. This milestone does not claim those features are complete.

## POP3 incoming mail and supported-provider picker · 2026-11-04

- User explicitly requested POP alongside IMAP and removal of unavailable provider choices. Added incomingProtocol (legacy defaults to IMAP), secure POP3 on the existing mailio transport, a protocol selector and POP presets. Microsoft/Exchange is removed from the picker; iCloud remains IMAP-only.
- POP requires stable UIDL, bounds lists/downloads, re-resolves opaque IDs per session, uses TOP with RETR fallback, validates TLS/STLS and never sends DELE. Bodies reuse the MIME/CID sanitizer and remote-image policy.
- Local read/star/delete state survives refresh/restart. Schema 6 adds account-scoped deletion tombstones; account/endpoint changes clear them with their cache. Local cached rows survive server removal and paginate independently of the server. SMTP-accepted POP messages get a local Sent copy, preserving accepted/unknown retry rules.
- Initial protocol run passed including renumbering, missing/duplicate/invalid UIDL, TOP fallback, dot unstuffing, limits and certificate/STLS rejection. Core/controller/UI runs passed 3/3 before the final pagination follow-up. Full VM suite passed 10/10 in 111.88s. Final Sent-cache/UI refinements passed controller/UI/i18n/catalog checks 4/4 in 59.91s. UI pin gate and 32 dependency guards passed; synthetic account-dialog capture reviewed. Final POP SMTP/folder protocol follow-up passed in 31.82s.
- Boundaries: no POP server deletion, remote folders/move/archive/server search, inactive-account unread polling or POP notifications. Downloaded bodies remain local; headers alone cannot recover a server-removed body. No real mailbox writes or host automated verification.

## Apply canonical settings-form components · 2026-11-04

- User identified account-field/select/choice styling inconsistent with the shared specification. Source audit confirmed explicit Settings.md rules: grouped SettingsTextField/SettingsSwitch and the value-only LiquidSelect settings variant. The app had selected generic capsule/checkbox/combobox controls instead. No shared library or pin changes are needed.
- Converted account fields and display toggles, retained owner-controlled state, added explicit switch accessibility names, corrected the protocol select and removed the account-dialog opacity override. Provider rows now derive from the actual catalog and use exclusive radio semantics; this also removes the stale undefined provider row left by the previous removal.
- VM captures reviewed for light/dark forms and expanded select; focused editing/binding/provider/narrow-layout checks passed. The initial full UI run found only an unsupported keyClicks helper in the new test; corrected to keyClick and the focused checks passed. Final VM UI/i18n/catalog checks passed 3/3 in 56.22s. The upstream pin check remains blocked by repeated GitHub API TLS EOF failures (also without proxy environment). No integration PR is created until that required check succeeds; no checks bypassed. Host preview rebuilt/replaced for manual inspection.

- Address placeholder follow-up: the new-account email hint is a real placeholder, now hidden whenever the field has active input focus and restored on blur while empty. Existing/typed address text is preserved. Validation uses the existing VM account editing and wizard regressions; no new style-only test was added.

- Grouped input follow-up: MailSettingsInput is a thin preset of canonical SettingsTextField (filled, white in light mode, dark-theme surface retained). Account row editors share its compact 24px/6px style; standalone email block stays unchanged. VM account editing/binding and wizard failure regressions passed (4/4 including lifecycle); host preview rebuilt and replaced for manual use.

- Grouped input sizing: reduced MailSettingsInput from 24px to 20px and vertical padding to 1px, retaining 12px text and the existing row layout.
  VM account editing/binding regression passed (3/3 including lifecycle). Host preview rebuilt and replaced for user inspection.

- Grouped input colors: default #d1d1d6, active input focus uses canonical AppTheme.defaultAccentSeed blue; contrasting text and inverted selection keep editing legible.
  VM account editing/binding regression passed (3/3 including lifecycle); host preview rebuilt and replaced for manual inspection.

- Corrected grouped input focus per user clarification: keep #d1d1d6 background and dark text, show only a 1px canonical-blue border on active input focus. This explicitly requested border supersedes the prior no-focus-border styling for this input.
  VM account editing/binding regression passed (3/3 including lifecycle); host preview rebuilt and replaced for manual inspection.

## Multi-account recent inbox synchronization · 2026-11-04

- Unified-inbox Refresh now downloads the newest 50 inbox headers per saved IMAP/POP account with available credentials. Periodic refresh also updates inactive-account inbox caches. The aggregate remains read-only; older uncached pages still require the source folder.
- InboxSync owns independent serial scheduling and per-account cache binding. Failures/missing passwords do not stop subsequent accounts; in-flight edited/deleted/cancelled results are discarded. Foreground writes cancel background results to avoid restoring stale flags. POP local flags/deletion tombstones survive refresh.
- Aggregate refresh preserves loaded pagination and the selected body, with explicit invalidation on source UIDVALIDITY changes. Progress uses the existing busy UI while source switching remains available. Synthetic regressions cover failures, credential isolation, missing credentials, POP reconciliation, deleted/edited accounts, cancellation and source switching during pending reads.
- Initial VM controller/i18n/catalog checks passed 3/3. Full VM suite passed 10/10 in 115.99s; final progress UI/controller/i18n/catalog checks passed 4/4 in 65.19s. Synthetic aggregate capture reviewed. Host preview rebuilt/replaced for manual inspection; no host automated validation. Canonical UI pin check and dependency guards now pass in the VM; previous GitHub API TLS blocker did not recur on this run.

- Previous settings iteration is now reviewable as PR #24, stacked on #23; the blocked dependency check was rerun successfully before PR creation.

## Automatic first receipt and discoverable refresh · 2026-11-04

- New password accounts now fetch their first inbox automatically. First-account refresh waits for queued session credential restoration and checks account/generation before starting; background additions refresh only their own inbox without switching the reader. Background refresh failures surface through the existing actionable error UI.
- Added a visible, labeled Check for mail action above the message list and completion-page guidance with Ctrl+R. iCloud guidance explicitly describes IMAP/SMTP with an app-specific password; this does not add Apple sign-in or claim provider acceptance.
- VM regression exposed the first-account credential timing race; fixed it rather than leaving a silent refresh failure. The updated synthetic test covers first receipt, background cache isolation and no SMTP submission. Full VM UI/controller/i18n/catalog suite passed 4/4 in 82.55s; UI pin and 32 dependency guards passed. Narrow-window action/busy-state coverage passed. No host compilation, app launch or automated checks.
- Remaining scope: aggregate mailbox writes, full-history/server reconciliation, junk/copy/.eml workflows, IDLE/POP notifications, disk image cache and favorites; real-provider acceptance and release-gate remain required.

## First-receipt progress/retry and unified inbox actions · 2026-11-05

- Found the earlier automatic-first-receipt commit already present. Replaced its static completion guidance with account-scoped loading/ready/failed state, folder discovery, explicit retry and Open inbox actions. Closing the dialog leaves receipt running. Background additions keep the existing reader; retry does not add another account or send SMTP mail. Removed completion-page Disconnect for unrelated accounts, duplicate errors and the misleading blanket read-only note.
- AggregateCommands partitions immutable source rows by account/folder/UIDVALIDITY, resolves scoped credentials and reuses MailboxCommands with a separate account-bound cache. Read/flag/delete/archive support mixed-account selections; move requires one account and uses that account's folders. Partial failures retain successful changes and report errors without retrying writes. POP local state and deletion tombstones remain intact.
- Opening an unread aggregate row can automatically mark it read. Credential restoration may resume its selected pending body while that flag command is queued; removal still invalidates the selection token. Added a regression for this ordering as well as first-receipt folder/header failures/retry, equal UIDs across accounts, partial failures for all four operations, source-specific moves, mixed-account move rejection and POP local deletion.
- Initial runs exposed obsolete read-only status assertions and a UI fixture without a destination folder; updated those fixtures for the new behavior. Controller suite and focused receipt/aggregate UI checks then passed. Full VM suite passed 10/10 in 121.98s; final completion-page UI/controller/i18n/catalog checks passed 4/4 in 72.88s; the focused capture run also passed (4/4 including lifecycle). Host preview rebuilt/replaced for user inspection. Canonical UI pin gate and 32 dependency guards passed. Synthetic screenshots reviewed; no real-mail writes or host automated verification.

## Deferred smart mailbox navigation · 2026-11-05

- User requested hiding the smart-mailbox section (All inboxes, Unread, With attachments) and deferring aggregation work. Sidebar entry points are hidden; existing backend/cache support is retained without further aggregate development.
- Favorites remain account-scoped: Inbox is the current account's inbox; Flagged filters cached messages across that account's folders; Local drafts queries drafts for the bound account.
- [ ] Redesign sidebar hierarchy with clear section headings, expandable parents and indented children; distinguish unified mailboxes from rule-based smart mailboxes.
- [ ] Define consistent account scope and cache/synchronization semantics for Unread/With attachments before restoring entries.
- [ ] Implement configurable smart mailbox rules and saved conditions before presenting this as a complete Smart Mailboxes feature. Resume only when requested.
- VM sidebar/retained aggregate UI checks passed (4/4 including lifecycle). Host preview rebuilt and replaced for manual use.

## Notification table word wrapping · 2026-11-05

- Reproduced vertical-letter status columns with synthetic 1%-width notification columns. The renderer's global overflow-wrap:anywhere reduced intrinsic minimum widths to glyph size; the new regression failed before the fix and passed after changing the envelope default to break-word.
- Preserve sender styles and table layout. Track rendered width and expose native scrollbars for intrinsically wide content, retaining existing long-document scrolling. Long prose still wraps; switching documents resets measured width. No sanitizer/network-policy/cache-format changes or real-mail artifacts.
- VM reader/core checks passed 2/2 in 5.91s, including wide/narrow word geometry, long prose, last-column horizontal reachability, reset, isolation and retained surfaces. Synthetic screenshot check passed (3/3 including lifecycle); both captures reviewed. Canonical UI pin gate passed. Host update is for manual inspection only.

## Favorite account labels · 2026-11-05

- Append the owning email address to built-in and custom favorite labels. Current-account shortcuts update with account changes; custom favorites retain their own source scope. Long labels elide and expose the complete label through the canonical tooltip. Keep the section title unscoped because custom favorites can belong to other accounts.
- VM account/section checks passed (4/4 including lifecycle); final account-switch/custom-favorite scope regression passed (3/3 including lifecycle). Canonical UI pin gate passed. Host preview rebuilt/replaced for manual use.

## Sidebar account hierarchy · 2026-11-05

- Add an Accounts section heading aligned and styled identically to Favorites. Expandable account rows remain children of Accounts, with their folders one level further indented. Replace the muted custom MouseArea add-account row with a shared MailAction, retaining keyboard/accessibility behavior. Following user feedback, present it as left-aligned blue plus-and-text without a filled background.
- VM UI/i18n/catalog passed 3/3 in 59.98s; hierarchy/disclosure follow-up 4/4 and final caption/capture 3/3 including lifecycle passed. Initial capture exposed an icon-only shared content layout; final icon-and-text adapter verified visually. Canonical UI pin gate passed. Host preview rebuilt/replaced for manual inspection.

- Plain blue Add account follow-up: VM sidebar and empty-profile account-entry checks passed (4/4 including lifecycle), synthetic capture refreshed, host preview rebuilt/replaced for manual use.

## Mail usability review follow-up · 2026-11-05

- Separate current-account shortcuts from fixed favorites, with a shared current address and secondary address lines for custom favorites. Preserve hidden aggregate navigation. Add full-account tooltips, explicit count meanings and muted draft totals.
- Expose existing account/folder/favorite menus with direct options actions; preserve account disclosure behavior. Prioritize replies at ordinary widths and compact to icons when needed. Consolidate direct refresh in the list header with account scope and checking state.
- Empty lists show errors and safe mail-check retry; do not replay failed mailbox writes. Add menu, scope, reply/refresh and empty-error regression coverage.
- VM final UI/i18n/catalog checks passed 3/3 in 62.43s; focused checks passed 5/5. Initial new test failed to locate a reparented popup; fixed test access and verified menu opening. Canonical UI gate passed. Synthetic capture reviewed. Host binary rebuilt and owned preview replaced for manual use only; no host tests or GUI verification.

## First distributable preview · 2026-11-05

- Integrated stacked PRs #19–#30 through PR #30 into main at 833e5b3c4aa401fac86a195781b9dabbbffa794b, preserving feature commits and the required release gate.
- Arch x86_64 0.1.0rc1 packaging and complete source export include pinned UI sources, licenses, checksums, dependency manifest and installation instructions. Packaging builds without OAuth fixture credentials and smoke-tests the installed binary in CI. Wayland runtime is declared explicitly.
- Clean PR acceptance: release-gate 10/10 passed in 106.74s, package built from the exported source, installed and passed CLI smoke tests (Actions run 42042100544). Main release build and artifacts are tracked by Actions run 42056400794. Publication uses that exact main commit and the v0.1.0-rc1 prerelease; see GitHub Releases for downloadable assets. No host execution/verification for this release work.

## Complete mail reminders · 2026-11-06

- IMAP/POP receipt feeds durable MailAlerts cursors independently of the viewed folder. Persist notification preference per profile; baseline quietly on first receipt/identity changes and deduplicate across refresh/restart/disabled periods.
- Asynchronous desktop notification actions retain exact account/endpoint/folder/epoch/UID targets, reject stale targets, restore the reader and wait for pending writes/sends. Account credential restoration resumes body loading and read updates.
- Tray badge totals locally synced inbox unread counts, updates on read/delete/account removal, caps drawing at 99+ with exact tooltip, and clears at zero. Retain five-minute polling and bounded 50-header receipt.
- VM full suite passed 11/11 in 134.11s; synthetic tray captures reviewed. Added policy, desktop D-Bus, cross-account navigation and unread-count regressions. Final focused checks passed 4/4 in 11.68s and narrow reader checks passed 3/3 including lifecycle; canonical UI pin gate passed. User explicitly authorized a companion Shell bridge fix; its isolated VM QML regression passes. Existing Release/tag remains unchanged; no host validation or real-mail writes.

## 2026-11-11 cross-application KWin ownership follow-up

### Verified publication and integration

- kos-apps v0.2.3 is published from 0d8ebada1d8e94e3426e005047e2d2329e578b5f. Native main run 42867328985 and signed publishing run 42867329067 both succeeded. The three final signed Release bundles (Calendar, Calculator, Weather) matched their release plans/server digests and each passed a fresh offline Wayland/KWin installation check, then was uninstalled with data retained.
- NkNotes v0.1.1 is published from 2c92cd529e360c5049f42e734defcc45cf024e81. Main run 42866965018 and draft publisher 42869343997 succeeded. All eight draft asset sizes/hashes matched the main artifact; the exact main app bundle passed offline KWin acceptance before publication. Tag and release target were verified.
- All four new Flatpaks use runtime/org.kde.Platform/x86_64/6.11. Their canonical native protocol module remains pinned at d7b34a8e8b37a9fb7605333f10d02f7b16e70743. Catalog URLs resolve latest Release plans; catalog signing remains user-owned.
- ListenFree PR #2 merged as 52c2571e5b8dc463e021da41cd7199c9e123ecba; main run 42867925615 succeeded. This is a source integration, not a Flatpak release.
- On 2026-11-11 the user explicitly approved merging verified Music PR #9. After rechecking the unchanged head 9c44b6b0128b4edd03f3b9ff96059dafd8054911 and passing PR CI, it was merged normally as 4c6c4d4ca86e4738342b1bd488e64c8e403a06a4. Main Window controls CI 42932700213 completed successfully, including the pinned-component build and takeover/layout/fallback regressions. The authorized merge is complete. Music still has no published Flatpak.
- The task's owned nested compositor was stopped through nested-stop. Owned previews and temporary app installs were removed; no host apps or reference working trees were modified/launched. Primary NkMail checkout is clean. Local synthetic fixtures and private captures remain under tmp/ and the VM validation task root for recovery.


## Upstream-main UI pin policy · 2026-11-11

- User explicitly removed the upstream Release requirement. Deliberately pin a tested full SHA already included in canonical nextkdeAppDemo main; do not follow a floating branch or automatically upgrade.
- Align contributor instructions, README, testing guide and CI terminology. Strengthen the existing commit-existence check with upstream-main ancestry verification; keep staged-lock/gitlink consistency, clean submodule, canonical URL, optional tag resolution and network-failure checks.
- Add guard regressions for main head/ancestors, existing commits outside main, malformed ancestry responses and API failure. VM guard regressions passed 36/36 and the live upstream-main check passed for the existing e2592b2742c58c1bd797a8bcfa2a3ae7da4affd8 pin. PR integration and CI remain pending.
- Current local icon SHAs 998c145 and 2ba0039 were not retrievable from upstream GitHub during this review. This is separate from Release policy: icon work must first be integrated into canonical upstream main. Preserve the concurrently edited original NkMail tree; do not substitute an unrelated main SHA or publish a release.
