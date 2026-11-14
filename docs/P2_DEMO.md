# P2 확인용 데모

프로젝트 폴더 `D:\바탕화면\Octopus_Simulator`의 **run_p2_demo.cmd를 더블클릭**한다.
설치된 Godot 4.6.2로 실행한다. 준비된 기록을 사용하므로 매번 C++ 빌드하거나
수 분짜리 검사 전체를 실행할 필요가 없다.

오른쪽 버튼으로 다음 장면을 고른다.

| 버튼 | 확인할 것 |
|---|---|
| Arm bending | 팔이 휜다. 금색 점은 최초 팔끝 위치이다. |
| Arm extension | 길이와 반지름 변화. 이동이 작으므로 수치와 확대를 함께 본다. |
| Pull a light box | 몸통과 0.5 kg 상자가 함께 이동한다. |
| Pull a heavy box | 같은 근육 명령에서 8 kg 상자와 몸통의 이동량이 달라진다. |
| Eight arms / 128 cups | 실제 8개 rod의 상태. 이 fixture는 변위가 작다. 빨판 테두리는 렌더링하지 않는다. |

- 화면 드래그: 카메라 회전, 휠: 확대/축소.
- Space / Pause: 재생·정지. Restart: 처음으로. 0.25x: 느리게.
- 슬라이더: 원하는 시간으로 이동. P: docs/preview에 스크린샷 저장.
- 오른쪽 수치는 원래 SI 좌표의 변위를 mm로 바꾼 값이다.

이 화면은 **실제 C++ 계산 결과의 기록 재생**이며 근육을 실시간으로 조작하는 게임은 아니다.
몸통 외형은 설명용 장식이며, 전체 화면에 공통 표시 배율/오프셋을 사용한다.
생체 타당성은 미검증이다. 기존 `run_preview.cmd`는 별도의 8팔·128빨판 시각 시제품이다.
단계 합격 근거는 화면만이 아니라 `progress/P2_COMPLETION.md`의 자동 검사다.

기록을 다시 계산하려면 프로젝트 폴더의 PowerShell에서:

```powershell
./tools/build-p2-demo.ps1
```

각 장면의 원시 기록과 수치·해시는 `game/data/demo/`에 저장된다.
모든 장면 성공 후에만 재생한다. 공식 도구 잠금·Git·실시간 연동 문제는 P0/P1의
별도 미해결 항목이며, 이 데모가 이를 해결했다고 표시하지 않는다.

## 실행 검증

다섯 장면을 Godot 4.6.2 / RTX 4060에서 실제 렌더링했다.
장면 전환, 시간 탐색, 정지 상태에서의 기록 메시 갱신을 확인했다.
로그: `progress/physics/P2-demo-renderer.log` (`P2_DEMO_ALL_OK cases=5`).
캡처: `preview/p2_demo_bend.png`, `p2_demo_extend.png`, `p2_demo_light.png`,
`p2_demo_heavy.png`, `p2_demo_eight.png`.
