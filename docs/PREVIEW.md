# 문어 시각 시제품

2026-11-14 · Godot 4.6.2 · **시각 시제품이며 실제 문어 물리 구현이 아님**

## 실행

가장 간단한 방법: 프로젝트 최상위의 `run_preview.cmd`를 더블클릭한다.
기존 `game/project.godot`의 기본 P0 장면은 그대로이며, 실행기가 별도
`res://scenes/octopus_preview.tscn`을 지정한다. C++ 빌드·Git·인터넷은 필요 없다.

PowerShell에서 프로젝트 폴더를 연 경우:

```powershell
.\tools\preview.ps1
```

설치된 엔진을 직접 지정하는 정확한 명령:

```powershell
& "C:\Tools\Godot\godot.exe" --path "D:\바탕화면\Octopus_Simulator\game" --single-threaded-scene --render-thread safe --resolution 1280x800 res://scenes/octopus_preview.tscn
```

## 조작

| 입력 | 동작 |
|---|---|
| 화면의 빈 곳에서 왼쪽/오른쪽 버튼 드래그 | 문어 주위를 도는 카메라 |
| 마우스 휠 | 확대/축소 |
| 1 / 2 / 3 또는 화면 버튼 | 기본 / 위에서 8개 팔 확인 / 낮은 빨판 시점 |
| Space | 팔 움직임·몸통 흔들림 일시정지/재개 |
| A | 자동 카메라 회전 켜기/끄기 |
| R | 기본 카메라로 복귀 |
| P | 현재 화면을 docs/preview/octopus_manual.png로 저장 |
| 창 닫기 | 종료 |

## 구현과 한계

몸통·머리·눈, 정확히 8개의 독립된 테이퍼 튜브 팔, 팔당 16개의 빨판
(두 줄 × 8개, 합계 128개)이 있다. 빨판은 돌출 고리와 안쪽 면으로 표현한다.
팔 중심선을 시간에 따라 변형하고 단면 프레임과 빨판 위치를 함께 갱신한다.
눈과 표면 무늬, 조명·그림자·받침대도 코드로 생성한다. 외부 에셋은 없다.

움직임은 사인파 기반 절차적 애니메이션이다. 근육·Cosserat rod·유체력·접촉·
흡착·반작용 계산이 아니며, 부유 모습도 물리 결과가 아니다. 실제 문어의 형태나
행동을 실측 검증하지 않았다. P0/P1 및 BIO 통과 근거로 사용할 수 없다.

## 실제 실행 확인

`./tools/preview.ps1 -Check`: Godot headless 장면 초기화, 팔/빨판 개수 검사 통과.
`./tools/preview.ps1 -Test`: 실제 OpenGL 창에서 렌더, 세 시점 캡처, 입력 처리
경로 검사 후 종료. 출력의 `PREVIEW_INPUT_CHECK_OK`와 stderr 없음 확인.

환경: Godot 4.6.2 stable official, OpenGL 3.3 Compatibility,
NVIDIA RTX 4060 / driver 610.88. 아래 파일들은 이미지 생성물이 아닌 실제
Godot viewport 캡처이며 각각 1280×800이다. 직접 이미지를 열어 외형을 검토했다.

- `preview/octopus_perspective.png`: 기본 시점
- `preview/octopus_eight_arms.png`: 위에서 정확히 8개 팔 확인
- `preview/octopus_suckers.png`: 낮은 시점에서 빨판 확인
- `preview/validation.json`: 노드 개수·팔 변형·카메라 드래그·줌·일시정지 검사 결과
- `preview/run.log`: 실제 렌더 및 검증 로그

입력 검사는 실제 입력 처리 함수에 이벤트를 넣어 상태 변화를 검사했다.
Windows UI 도구는 창 선택 오류로 실기 마우스 검증을 완료하지 못했다.
따라서 사람이 직접 마우스로 조작감을 평가했다는 주장은 하지 않는다.
정량적 프레임 성능이나 물리 정확도도 이번 시제품의 검증 범위가 아니다.

## 별도로 남은 P0 문제

공식 도구 배포물의 해시/의존성 커밋 고정, 요구 Godot 4.7.2와 godot-cpp 확보,
GDExtension 왕복 검증, 원격 Git DNS 문제, `.git/index.lock` 쓰기 제한이 남아 있다.
이 시제품은 사용자 요청에 따라 그 문제와 독립적으로 추가했으며, 기존 P0 코어와
부트스트랩 장면·설정은 변경하지 않았다.
