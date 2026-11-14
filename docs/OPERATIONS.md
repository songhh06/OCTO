# OCTOPUS 자율 개발 운영 매뉴얼

## P0 bootstrap environment addendum (2026-11-14)

From the repository root, first dot-source `. ./tools/dev-shell.ps1`. It loads
MSVC's vendor environment, sets VSLANG=1033 (avoids the observed localized
compiler-prefix detection crash), and keeps Godot settings/temp inside work/.
CMake uses embedded /Z7 debug info to avoid the observed PDB-server C1902 in
this sandbox. These changes do not disable compiler working checks.

`./tools/verify.ps1` runs available dev/release tests plus the empty Godot scene.
`./tools/verify.ps1 -RequireCompleteP0` additionally requires official archive
verification and native GDExtension roundtrip. Only the latter can satisfy P0.
CTest must use `--no-tests=error` so an unconfigured directory cannot pass.

Observed local tools and unverified provenance are in toolchain.lock.json.
The installed Godot 4.6.2 is diagnostic only; the 4.7.2 product requirement is
unchanged. Until dependencies are downloaded and verified, a new checkout is
not claimed to be a complete reproducible P0 installation.

Additional sanitizer validation:

```powershell
. ./tools/dev-shell.ps1
cmake --preset asan
cmake --build --preset asan
ctest --preset asan --output-on-failure --no-tests=error
```

Godot diagnostic commands in this sandbox require
`--single-threaded-scene --render-thread safe` and PowerShell output piping
(`2>&1 | Out-Host`) for interactive diagnostics. The verification script uses
Start-Process -Wait with hidden window and captured output to reliably await the
GUI executable, inspect its exit code and require the expected scene marker.
For native smoke use `-- --bridge-smoke`; a missing extension must return 2.


버전 1.0 · 2026-11-14. 이 절차는 새 Windows 11 PC를 기본으로 썼다. macOS/Linux는 같은 버전·잠금·테스트 개념을 유지하되 명령은 해당 셸로 바꾼다. 설치 프로그램의 옵션·SHA256은 공식 출처에서 그날 확인한다. 임의의 해시나 존재하지 않는 패키지 버전을 채워 넣지 않는다.

## 1. 첫날 작업 순서

1. `git --version`, `cmake --version`, `python --version`, `clang --version` 또는 MSVC `cl`, `godot --version`, `blender --version`을 확인하고 결과를 `docs/progress/BOOTSTRAP.md`에 남긴다.
2. Godot 4.7.2 stable 공식 릴리스, 대응 `godot-cpp`, CMake, Ninja, C++20 컴파일러(MSVC 2022 또는 Clang), Python, Blender, Eigen, Catch2 또는 GoogleTest의 **정확한 버전·태그·소스 커밋·SHA256**을 `toolchain.lock.json`에 기록한다. 최신 버전을 자동 추종하지 않는다. 다운로드 URL과 라이선스를 기록한다. Godot과 `godot-cpp` API 버전 호환성을 최소 확장 빌드로 검증한다.
3. 개인 계정·원격 인증 없이 로컬 새 저장소를 만든다. 문서와 빈 프로젝트 뼈대를 커밋한다. 기존 폴더가 있다면 먼저 `git status`, 파일 목록, 원격을 확인하고 사용자 변경을 보존한다.
4. `cmake --preset dev`, `cmake --build --preset dev`, `ctest --preset dev --output-on-failure`, Godot headless import, Godot 에디터 실행까지 확인한다.
5. P0 태스크를 종료한 뒤 P1을 시작한다. 이 순서 중 실패한 항목은 원인과 재현을 기록하고 해결한다.

### 1.1 잠금 파일 계약

`toolchain.lock.json` 예시 구조. 실제 값은 확인 후 채운다.

```json
{
  "schema": 1,
  "checked_at": "2026-11-14",
  "host": "windows-x86_64",
  "tools": {
    "godot": {"version": "4.7.2-stable", "url": "OFFICIAL_URL", "sha256": "VERIFY_ME"},
    "godot_cpp": {"commit": "VERIFY_ME", "url": "OFFICIAL_URL"},
    "cmake": {"version": "VERIFY_ME"},
    "compiler": {"name": "MSVC-or-Clang", "version": "VERIFY_ME"},
    "ninja": {"version": "VERIFY_ME"},
    "eigen": {"commit": "VERIFY_ME"},
    "test_framework": {"name": "Catch2-or-GTest", "commit": "VERIFY_ME"}
  }
}
```

`VERIFY_ME`가 하나라도 있으면 P0 미완료다. 의존성 다운로드는 CMake `FetchContent`의 태그 대신 해시 고정 또는 `vcpkg` manifest와 baseline/lock으로 고정한다. CI 캐시가 달라도 같은 바이너리 원본을 받을 수 있어야 한다. 설치가 불가능한 도구는 대체 버전의 호환성 증거와 결정 기록을 남긴다.

## 2. 신규 리포지토리 생성

권장 구조:

```text
octopus/
  AGENTS.md                  # 이 문서를 복사
  CMakeLists.txt
  CMakePresets.json
  toolchain.lock.json
  .gitignore
  .github/workflows/ci.yml
  docs/GAME_SPEC.md ROADMAP.md OPERATIONS.md EVIDENCE.md
  docs/DECISIONS.md docs/progress/STATUS.md
  core/include/octo/ core/src/
  godot_bridge/src/ godot_bridge/gdextension/
  game/project.godot game/scenes/ game/scripts/ game/shaders/
  assets/source/ assets/export/ assets/ASSET_MANIFEST.md
  tests/unit/ tests/integration/ tests/scenes/
  tools/ profiles/ replays/golden/
```

PowerShell에서 빈 폴더를 만들 때는 사용자 지정 목표 경로를 먼저 확인한다. 예시는 `octopus`라는 새 폴더를 현재 위치에 만드는 경우다.

```powershell
New-Item -ItemType Directory -Path .\octopus | Out-Null
Set-Location .\octopus
git init -b main
git config user.name
git config user.email
```

Git 신원 미설정이면 사용자 이름/이메일을 임의로 만들지 말고 로컬 설정값을 요청한다. `.gitignore`는 `build/`, `.godot/`, `*.pdb`, IDE 임시 파일, 사용자별 설정, 로그·캐시를 제외한다. `assets/source/`, 잠금 파일, 테스트 입력·golden replay, `project.godot`, GDExtension 설정은 추적한다. 큰 바이너리는 Git LFS 사용 여부를 권리·저장소 정책과 함께 결정하고, 원격이 없으면 일단 생성 자산과 압축 중간 파일을 제외한다.

초기 커밋 뒤 `feature/P0-bootstrap` 브랜치를 만든다. 각 태스크는 해당 브랜치에서 커밋하고 단계 수용 후 `main`에 로컬 fast-forward 또는 검토 가능한 merge를 한다. 외부 푸시와 공개 PR은 별도 게이트다. 이슈는 원격 서비스가 없으면 `docs/issues/NNN-title.md`로 기록하고, 재현/기대/실제/로그/원인/해결/검증을 포함한다.

## 3. 빌드와 실행 명령 계약

P0가 아래 명령을 구현한다. preset 이름을 바꾸려면 이 문서·CI·README를 함께 수정한다.

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
cmake --preset release
cmake --build --preset release
ctest --preset release --output-on-failure
godot --headless --path game --editor --quit
godot --path game
```

CLI 물리 검증의 표준 형태:

```powershell
build\dev\octo_cli.exe --scene tests/scenes/V09_closed_world.json --profile profiles/default.toml --seed 42 --ticks 600 --stats work/V09.json --replay work/V09.octoreplay
```

실행 파일의 실제 위치는 CMake preset으로 고정한다. `work/`는 임시 결과로 커밋하지 않는다. 실패하면 표준 오류와 stats/replay를 저장하고 CI artifact에 첨부한다. `octo_cli --validate-scene`으로 형식·단위·음수 질량·누락 재질을 사전에 검사한다. 개발 빌드는 AddressSanitizer/UBSan 또는 Windows에서 이용 가능한 동등 검사 빌드를 별도 preset으로 제공한다. 릴리스 빌드는 디버그 검증을 없애도 수치 불변량 감시는 유지한다.

## 4. CI 설계

`.github/workflows/ci.yml`은 로컬에서 준비하되 원격 업로드 승인 전에는 실행되었다고 표시하지 않는다. 트리거: push/PR, Windows 최신 호스트. 단계: checkout → 잠금 파일 검증 → 공식 해시 확인/도구 준비 → dev 구성·빌드 → 단위 및 V01–V12 통합 테스트 → Godot headless import → scene schema 검사 → 산출 로그 업로드. Linux Clang 빌드는 교차 플랫폼 회귀용 추가 잡. 대표 성능 V13은 공유 CI 머신의 변동이 커서 합격 판정은 고정 개발 머신에서 하고, CI에는 회귀 경향만 기록한다. V14 실기 플레이는 CI 합격으로 위장하지 않는다.

CI 성공 조건: 모든 테스트 통과, 미해결 sanitizer 오류 0, 잠금 해시 불일치 0, headless import 오류 0. 실패 시 빨간 CI를 우회하는 `continue-on-error`를 핵심 테스트에 쓰지 않는다. 실패한 재현 장면·stats·버전 정보를 artifact로 보존한다.

## 5. 에셋 파이프라인

Blender 소스 `.blend`와 생성 스크립트·내보내기 설정을 `assets/source/`에 둔다. glTF 2.0/GLB로 메쉬·재질·노드 이름을 내보내고 Godot에서 import한다. 좌표축·스케일 변환을 단위 큐브/1 m 막대로 자동 검사한다. 기본 팔 메시에는 요소별 단면 프레임에 맞는 버텍스 가중치나 절차적 튜브 생성기를 사용한다. 빨판 고해상도 모델·LOD·충돌 표면의 물리 형상은 별도 파일로 관리한다. 텍스처 압축과 셰이더 변형은 대표 하드웨어 GPU 메모리로 검증한다.

`assets/ASSET_MANIFEST.md`의 각 행: 파일 경로, 원본/생성 방식, 제작자, 라이선스, 사용 조건, 다운로드 URL 또는 생성 명령, 변경 이력. 라이선스 불명 에셋은 저장소와 빌드에 넣지 않는다. 생성형 이미지/사운드를 사용할 경우 사용 도구·입력 요약·상업 이용 조건 확인 결과를 기록한다. Blender 및 외부 음원/모델의 실제 사용 조건은 해당 시점에 재확인한다.

## 6. 매일 진행 기록과 이슈 해결

작업 시작: `git status` → `docs/progress/STATUS.md`의 마지막 단계와 실패 확인 → 다음 한 태스크 선정. 작업 종료: 테스트 결과·성능·리플레이·변경 파일·커밋 해시 업데이트. `STATUS.md` 서식:

```text
날짜 / 브랜치 / 커밋:
목표와 완료 기준:
구현·에셋 변경:
실행 명령과 통과/실패(V ID 포함):
성능 장면·하드웨어·p50/p95:
남은 이슈(최소 재현·로그):
미검증 물성·의사 결정:
다음 태스크:
```

오류 처리 순서: 최소 장면으로 재현 → 입력·시드·버전 저장 → 유한값/단위/좌표계/압력 부호 확인 → 접촉 후보와 Jacobian 수치 차분 확인 → 시간 단계 절반에서 변화 확인 → 수렴 로그·에너지·모멘텀 분석 → 원인 수정 → 실패 장면을 회귀 테스트로 등록. 크래시만 사라졌다고 해결 완료가 아니다. 성능 문제는 프로파일로 병목을 밝히고 O(N²) 접촉 후보, 메모리 할당, sparse solve, 렌더 upload, 빨판 LOD 순으로 확인한다. 물리 정확도를 낮추는 변경은 V01–V12 회귀 후 적용한다.

## 7. 배포 후보와 승인 경계

릴리스 스크립트는 잠긴 도구로 Windows 빌드, Godot export, 실행 파일·필수 라이브러리·라이선스·README·알려진 문제·SHA256 목록을 한 폴더에 만든다. 깨끗한 PC/VM에서 압축 해제→시작→탱크 레벨→종료→로그 확인을 수행한다. 실제 게임패드, 사람 플레이 감각, 생체 타당성은 자동으로 합격할 수 없다. 로컬 패키지는 끝까지 준비하고 외부 저장소 공개·배포 플랫폼 업로드·유료 구매는 `AGENTS.md`의 승인 게이트에서 최종 결정을 받는다.


## 체적 연성체 회귀 추가

추가 명세의 V16–V21, FEM 요소 독립 차분, 메시 품질·압력 안정화/세분화,
mass ledger, rod/cage 모드 분리, 피부 embedding 검사와 기존 V01–V12를 유지한다.
같은 검사 명령을 로컬/CI에서 실행한다. 실패 tick 전체 rollback에 최소 J,
압력·접촉 feature·잔차·입력 리플레이를 남긴다. Godot SoftBody3D를 별도
진실원으로 병렬 적분하지 않는다. 새 대표 장면 p50/p95·메모리를 기록한다.
