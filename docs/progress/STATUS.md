# STATUS

## 2026-11-14 — PR #2: GameReady rig / dense contact / monolithic torso FEM

**Work in progress; P2V full acceptance remains OPEN.**
Branch `feature/P2V-model-curved-contact-body`, stacked draft PR #2.

- Uploaded user GLB: glTF2, 8×24 arm bones, 193 total skin joints,
  12,698 main skin vertices. Original 15,203,672-byte binary remains
  **outside the Git branch**; must be copied to
  `game/assets/Octopus_GameReady_v5.glb` from the user's installer ZIP.
  Godot adapter maps reversed art/native arm winding, drives 192 bones from
  native snapshots, hides the old procedural display and skips its mesh rebuild.
- Godot CI imported a **synthetic**, same-named 193-bone GLB, then passed
  `LIVE_MODEL_LOADED`, `LIVE_FEM_SURFACE_READY`,
  `LIVE_GODOT_CONTACT_OK` and `LIVE_MODEL_SKIN_OK`:
  https://github.com/songhh06/OCTO/actions/runs/43593877339.
  The actual supplied asset has NOT received render/skin acceptance.
- C++ native FEM torso now owns seven mass-conserving shared-world vertices,
  eight tetrahedra and a condensed volumetric Schur block. The rod root shares
  a center degree of freedom; three compliant beak-to-mantle attachments
  transmit rotational torque. A moving eight-triangle surface generates
  barycentric box/plane/terrain contact, with bilateral rigid-box reaction.
  Godot F3 displays native FEM boundary (debug view). Surface-contact/
  torque tests were added after the initial passing FEM CI.
- Reference 120-tick native FEM protocol and unit tests passed on Windows:
  https://github.com/songhh06/OCTO/actions/runs/43591573923.
- Previously failing curved 100k-triangle benchmark now **converges all
  10 ticks** with original `dual_tolerance=0.002`, not loosened.
  Latest confirmed run: https://github.com/songhh06/OCTO/actions/runs/43593872655.
  p95 ~19,786 ms/tick: 120 Hz target 8.333 ms **NOT MET**.
  Dense-terrain swept CCD, arbitrary contact endurance remain open.
- **Not achieved:** actual user GLB GPU-render inspection; high-resolution
  FEM body skin embedding; deformable arm cross sections and suction patches;
  mantle internal chamber and beak clearance; V16–V21; V13 speed/BIO.
  Numerical parameters are ASSUMED. See issues/004 and issues/007.
- Run `.github/workflows/live-regression.yml` and
  `.github/workflows/curved-contact.yml`; use latest GitHub CI run on
  PR #2 before changing draft status.

## Live transport and reset hardening — Windows CI verified (scoped)

Branch `feature/P0-live-process-recovery`; PR #1 (draft).
Native child death and stopped/in-flight reset are recoverable from R/Reset;
malformed live RPC now returns a structured error without killing the solver.
`run_live_demo.cmd` rebuilds native changes rather than reusing an old executable.
Stale checked-in CMake caches are discarded when the checkout path changes.
At commit `f09fcf1`, Windows Actions passed 28/28 short CTests,
1/1 360-tick native live protocol test, and Godot 4.6.2 child-death and
stalled-request restart smokes (no engine/script errors). Subsequent
launch-script and full Godot contact changes are awaiting their own latest CI
result. See issues/006 and .github/workflows/live-regression.yml.
**Issue 004 curved-contact nonconvergence and V13 performance are still open.**

## Live contact convergence fix — verified
Saved tick-319 wrapping failure reproduced: CCD/resting normal alternation caused
a dual two-cycle, not a failed Newton residual. Fix keeps current geometric normals
for active resting capsule pairs and preserves swept orientation for true crossings.
Saved state + 60 released ticks pass Release/Debug/ASan.
Full Release 36/36, Debug/ASan 27/27; static analysis/formatting and Godot
drag/contact/pause/reset passed. See issues/005 and physics/live-contact-fix-*.log.
Diagnostics now save before response; Windows diagnostic argv uses wmain.


## Live Godot box-contact demo — VERIFIED

User-prioritized live connection works through asynchronous native stdio RPC.
Double-click run_live_demo.cmd: drag box or WASD/arrows, Q/E, pause/reset.
Native single-world box/8-rod collision; no recorded file and no pose overwrite.
Godot 4.6.2 verified drag-target input, Space pause, contact and reset; real capture:
docs/preview/live_box_contact.png. Native no-contact/contact trials: 360 ticks each,
zero baseline motion vs ~0.329 m contact deformation, penetration <2mm.
Release short regressions 26/26 + live.protocol passed; ASan 8-tick transport smoke
and clang-tidy passed. See docs/LIVE_DEMO.md and physics/live-*.log/json.
This is a rigid-torso 12-element/arm interaction demo, not FEM/P2 completion,
not 120 Hz performance acceptance, and not GDExtension/provenance completion.


## P2 전체 보완 + 체적 연성체 — 진행 중

사용자 추가 문서 두 개를 전체 확인하고 docs에 원문 보존했다.
기존 V06–V12 통과 기록과 rod/빨판/데모는 보존하되 전체 완료 표시는 철회한다.
확인된 곡면 수렴·충돌 범위·실시간 연결·성능 문제와 P1V/P2V, V16–V21을
해결하고 함께 검증해야 한다. 새 기준 비교: P2V_GAP_ANALYSIS.md.
첫 구현 추가: 독립 사면체 mixed u-p 요소, 힘·Jacobian 독립 차분 및 내부 embedding.
FEM.P1V1: Release/Debug/ASan 통과, clang-tidy 통과.
아직 mesh 압력 모드/세분화·시간 적분·전체 세계 접합이 없으므로 V16–V21 통과 아님.
검증 명령: FEM CTest 타깃(구현 후 등록), 기존 ctest --preset release.

---

## 이전 결과 — 아래 완료 표기는 당시 지정 검사 통과 기록


## P2 inspection demo — verified
Double-click run_p2_demo.cmd. Five native recorded cases with selection, timeline, pause, slow playback, reference marker and numeric displacement. All five rendered/captured in Godot 4.6.2; P2_DEMO_ALL_OK. See docs/P2_DEMO.md and physics/P2-demo-renderer.log. P0 and separate visual prototype preserved.


## 2026-11-14 — P2 네이티브 기능·지정 수용 검사 완료

P2.1–P2.6 구현과 V06–V12 검증 완료. P2.7 전체 규모 측정·목표 격차 기록 완료.
P0/P1 출처·라이브 Godot 연결·Git 문제로 전체 프로젝트 단계 통합 수용은 열려 있다.
Release 33/33, Debug/ASan 각 25/25, clang-format/clang-tidy, Godot 4.6.2
headless import/bootstrap 통과. 기존 P0/시각 시제품 보존. BIO 미검증.

8팔·128빨판 60초 이동/접촉/해제 검사: 최소 120개 부착,
최대 침투 1.99525e-08 m, p50/p95 47.6873/57.0959 ms.
5초 가벼운/무거운 상자 반작용, 10초 저장 복원, 1,200틱 입력 리플레이,
파일로 읽은 8팔 장면의 120틱 입력 리플레이도 통과.

재현: `./tools/verify.ps1`; 상세 명령·수치·한계는 `P2_COMPLETION.md`.
남은 문제: V13 속도 목표 미달, 곡면 고부하 multiplier 수렴 실패(issues/004),
P0 도구 출처/의존성 검증·Git/DNS·호스팅 CI·실시간 GDExtension.
브랜치/커밋 쓰기 거부: P2-git-write.log. HEAD 4541a5975d62143c3cbf51a2c7a8cdd3adb81aac.
다음 순서: 상위 통합 문제 해결, P3 조작/게임화·성능 개선. P3/P4/BIO 승인 없음.

---

## 이전 기록 — 아래 부분 구현 표기는 위 최종 결과로 대체됨


## P1/P2 물리 개발 (2026-11-14)

독립 C++20 팔 역학·7채널 근육·Newton-CG·빨판 압력/박리·유체·강체 결합·
기본 접촉·저장 복원을 구현. **P1 핵심 V01–V05 통과, 단계 전체 완료는 아님.**
**P2는 부분 구현/검증이며 완료 아님.** 상세: progress/P1_P2_REPORT.md.

최종 검증: tools/verify.ps1 통과; Release 23/23, dev/ASan 각 17/17 선택 검사.
긴 동역학·60초 정지 유지·10초 복원 재현 시험은 Release에서 전체 실행.
clang-format/clang-tidy 및 기존 Godot import/빈 장면 회귀 통과.
8팔·128빨판 60초 **정지** 시험은 통과했지만, 움직이는 다중 접촉 V10은 미완료.
단일 팔–몸통–상자 5초 폐쇄 세계 반작용/질량 차이 시험 통과.
실제 네이티브 결과는 run_physics_lab.cmd로 **오프라인 재생**하여 확인 가능.

성능: Ryzen 5600X / Release / 8팔 정지 fixture p50 3.4389 ms, p95 4.3886 ms;
이동 단일팔–상자 진단은 약 12 ms p95로 8.33 ms 목표 초과. V13 통과 아님.
미완료: 실시간 GDExtension, swept capsule/CCD, 물성별 미끄럼 통합,
압력/박리 강결합·active-set 제약, 문 힌지, 일반 장면/프로파일·입력 리플레이.
자료/수식·한계: docs/PHYSICS_MODEL.md; 근거 수치는 모두 ASSUMED/BIO 미검증.
Git 새 브랜치/커밋 실패, 기존 브랜치의 작업 트리에 보존. HEAD는 초기 4541a59.
P0 도구 잠금·Git/DNS 문제는 여전히 별도 미해결. 릴리스 태그/외부 푸시 없음.

---

## 별도 시각 시제품 (사용자 추가 요청, 2026-11-14)

완료: Godot 4.6.2용 octopus_preview.tscn, 몸통·정확히 8팔·128빨판,
부드러운 절차적 팔 움직임, 회전/줌 카메라, 위/아래 시점, 일시정지,
스크린샷 저장, 최상위 run_preview.cmd 실행기.
실제 RTX 4060/OpenGL 렌더 및 세 장의 viewport 캡처를 직접 확인.
팔·빨판 개수 및 드래그/줌/일시정지 입력 함수 검사 통과.
기존 P0 장면·project.godot·C++ 코어 보존. P0 수용 상태는 아래와 같이 미완료.
실제 물리·생체 검증 완료 주장이 아닌 **시각 시제품**.
실행·조작·검증·남은 문제: docs/PREVIEW.md. 이미지: docs/preview/.
OS 마우스 실기 검증은 UI 도구 창 선택 실패로 미완료이며 입력 경로를 자동 검사함.

---

2026-11-14 / feature/P0-bootstrap / **P0 진행 중, 수용 불가**

목표: 도구 출처 검증, 독립 C++20 코어·CLI·테스트, 네이티브 확장 tick 왕복,
장면·리플레이 형식 고정. 상세 기준별 결과는 BOOTSTRAP.md 참고.

구현: 빈 C++ 세계의 120 Hz tick/스냅샷, CLI, CMake dev/release/asan,
형식·의미 검증, Godot 빈 3D 장면, 미컴파일 GDExtension 소스,
서식·정적 검사, CI 초안, 라이선스 초안. 기존 문서와 작업 보존.

검증: `./tools/verify.ps1` 통과. dev/release/ASan 각각 CTest 5/5,
clang-format/clang-tidy 통과, Godot headless import 및 P0_SCENE_READY 확인.
테스트 ID: P0.core_contract, P0.cli_tick, P0.cli_reject_negative,
P0.schemas, P0.core_engine_boundary. V01–V14는 미구현·미실행.

남은 실패: 공식 배포물 SHA256·다운로드 URL·의존성 커밋 검증 미완료;
Godot 설치본 4.6.2와 요구 4.7.2 불일치; 네이티브 왕복은
P0_BRIDGE_MISSING/exit 2; Git 원격 조회 DNS 실패/exit 128.
`python tools/verify_lock.py --require-verified`는 의도대로 exit 2.
원격 CI, 새 체크아웃 재현, 화면·사람 플레이 검증은 미완료.

성능: Ryzen 5 5600X / RTX 4060 / Windows 10.0.26200.
현재 빈 세계이므로 solver·렌더 p50/p95 및 물리 안정성 수치를 제시하지 않음.
BIO 미검증. 샘플 재질은 ASSUMED이며 실제 역학에 사용되지 않음.

Git: origin=https://github.com/Aggang-77/Octopus-simulator.git;
초기 main 4541a59. 원격 이력 미확인, fetch/push/merge/tag 없음.
현재 HEAD=4541a5975d62143c3cbf51a2c7a8cdd3adb81aac (초기 문서 커밋).
구현 커밋 시도는 `.git/index.lock: Permission denied`로 실패.
새 구현 및 기록은 작업 트리에 보존되어 있으나 미커밋이며, 작업 트리는 깨끗하지 않음.

다음: docs/issues/001-network-and-toolchain.md의 도구·네트워크 차단을 해결하고
정확한 도구 잠금·확장 빌드·tick 왕복·깨끗한 체크아웃 검증을 수행.
Git 메타데이터 쓰기 제한도 해결해야 함 (docs/issues/002-git-write.md).
P0 통과 전 P1–P4로 진행하지 않음. 승인 게이트는 임의 통과 처리하지 않음.
