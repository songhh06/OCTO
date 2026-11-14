# 네이티브 물리 실험 장면

`run_physics_lab.cmd`를 더블클릭하면 최근 생성한 C++ 물리 결과를 Godot에서
재생한다. 기본 기록은 몸통–24요소 팔–상자가 양방향으로 반응하는 실험이다.
원래 8팔 시각 시제품은 `run_preview.cmd`로 그대로 실행할 수 있다.

**실시간 물리 게임이 아닌 C++ 계산 결과의 오프라인 재생**이다. 입력값을
바꾸어 새 계산을 하려면 프로젝트 루트의 PowerShell에서 아래 명령을 실행한다.

```powershell
# 몸통·팔·0.5 kg 상자의 결합, 5초 계산 후 재생
.\tools\physics-lab.ps1 -Scenario coupled -Command 0.04 -Mass 0.5

# 단일 팔의 +X 굽힘, 반대 굽힘은 Channel 2
.\tools\physics-lab.ps1 -Scenario arm -Channel 0 -Command 0.12

# 횡근 신장 / 사근 비틀림
.\tools\physics-lab.ps1 -Scenario arm -Channel 4 -Command 0.15
.\tools\physics-lab.ps1 -Scenario arm -Channel 5 -Command 0.04

# 8팔·128빨판 정지 유지 기록 (활성 이동 스트레스 시험 아님)
.\tools\physics-lab.ps1 -Scenario eight -Ticks 7200
```

카메라 드래그·휠·1/2/3 시점, Space 일시정지는 시각 미리보기와 동일하다.
근육 입력은 위 명령으로 새 trace를 생성할 때 적용된다. 화면에서는 위치를
덮어써 물리를 흉내 내지 않고, 저장된 네이티브 위치·회전·반경만 보간한다.
원래 시각 시제품의 장식 빨판은 물리 재생에서 숨긴다. 리셋/재생 반복은
물리 시뮬레이션의 새 실행이 아니라 기록 재생이다.

`-Capture`는 실제 렌더 화면을 docs/preview/native_physics_lab.png로 저장하고
종료한다. 성공 trace에만 재생을 허용한다. 실패하면 checkpoint와 stats를
남기며 오류를 반환한다. 파일 위치와 SHA256은 docs/progress/physics/manifest.json,
모델 및 수용 제한은 PHYSICS_MODEL.md와 progress/P1_P2_REPORT.md를 참고한다.

아직 물리 모델·실기 조작·생체 타당성 검증이 끝난 게임은 아니다.
