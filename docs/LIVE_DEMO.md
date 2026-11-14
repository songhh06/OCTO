# 실시간 Godot 상자 접촉 데모

프로젝트 폴더 `D:\바탕화면\Octopus_Simulator`에서 **run_live_demo.cmd**를 더블클릭한다.
기존 `run_p2_demo.cmd`는 기록 재생이며 이 새 실행기는 실시간 연결이다.

## 조작

- 파란 상자를 왼쪽 마우스로 잡고 드래그하거나, A/D·W/S 또는 방향키로 움직인다.
- Q/E: 낮추기/올리기. 오른쪽 마우스 드래그: 카메라 회전. 휠: 확대/축소.
- Space 또는 Pause: 정지·재개. R 또는 Reset world: 계산 세계 초기화.
- 금색 반투명 상자는 요청한 위치, 파란 상자는 계산된 실제 위치다.
- 처음에는 A를 눌러 상자를 문어 쪽으로 밀면 오른쪽 앞 팔과 부딪혀 팔이 휜다.
  접촉에 막히면 파란 상자가 금색 목표까지 따라오지 못하는 것이 정상이다.

## 실제 계산 경로

Godot 4.6.2의 OS.execute_with_pipe(..., false)로 로컬 C++ 프로세스를 실행한다.
Godot는 JSON 명령을 보내고 비동기로 최신 스냅샷을 읽는다. 코어가 상자·몸통·8팔을
단일 물리 세계에서 함께 적분한다. Godot RigidBody3D와 병렬 적분하지 않는다.
화면은 스냅샷 보간만 수행하며, 팔 위치를 입력/애니메이션으로 덮어쓰지 않는다.
공식 API: https://docs.godotengine.org/en/4.6/classes/class_os.html#class-os-method-execute-with-pipe
파이프 읽기 가능 바이트: https://docs.godotengine.org/en/4.6/classes/class_fileaccess.html#class-fileaccess-method-get-length

전송 규약 v1: 한 줄에 UTF-8 JSON 하나. 요청 op=step/reset/quit.
step: target=[x,y,z] (SI m), drive=true. 응답: protocol, live, ok, error,
contacts, penetration(m), solver_ms, arm_motion(m), snapshot(tick/body/arms/rigids).
동시에 요청 하나만 처리하며 큐가 쌓이지 않는다. snapshot tick은 1/120 s씩 증가한다.
목표 x/z는 [-1.6,1.6]m, y는 [-.15,.7]m로 제한한다. 입력은 외력형 손잡이:
30 N/m 위치 스프링, 10 N*s/m 감쇠, 최대 10 N. 상자를 세워 잡는 토크는
4 N*m/rad, .8 N*m*s/rad, 최대 2 N*m. 물체 pose를 강제로 이동시키지 않는다.
종료 시 소유한 자식 프로세스를 정리한다. 실패 시 해당 tick rollback 후 화면을 정지하며
work/live-failure.octostate를 남긴다. Reset으로 새 세계를 만들 수 있다.

## 범위와 한계

이는 **실시간 입력 연결**이며 120 Hz 실시간 성능 목표 달성이라는 뜻은 아니다.
현재 계산이 벽시계보다 느릴 수 있고 UI는 비동기로 유지된다. 우측에 계산/응답 ms와
실제 simulated time을 표시한다. 비교 데모는 팔당 12요소(8팔), 강체 몸통, 동적 상자,
무중력+유체 항력이다. 기존 24요소 기준·기록 재생·시각 시제품·FEM 작업은 보존했다.
몸통 외형은 설명용이며 실제 강체 구 충돌 형상과 다르다. 팔/상자 표면은 native 기록으로
그린다. 체적 몸통·cage·변형 빨판은 아직 연결되지 않았다. BIO는 미검증이다.
이 파이프 연결은 작동하지만 기존 P0의 GDExtension 라이브러리 빌드/출처 검증 게이트를
통과했다고 주장하지 않는다. 엔진은 설치된 Godot 4.6.2를 그대로 쓴다.

## 재현·검증

```powershell
./tools/build-live-demo.ps1
python tests/integration/live_bridge_tests.py build/release/octo_live.exe
```

native 대조 실험은 비접촉 360틱, 실제 접촉 360틱을 비교하고 접촉·변형·침투 <2mm,
reset tick=0을 검증한다. 결과: progress/physics/live-bridge-tests.json.
Godot에서는 드래그 목표 변경, Space 정지, 상자 자동 밀기, native 접촉/변형,
초기화 응답을 실제 실행해 확인한다. 로그: progress/physics/live-godot-test.log.
스크린샷: preview/live_box_contact.png.


## 접촉 오류 수정 (2026-11-14)

기존 `Newton-CG/line-search exhausted 1/2/4 subdivision` 메시지는 접촉 반력
반복 실패도 같은 문구로 표시했다. 저장된 팔 자기접촉 실패를 재현해 CCD와
기존 접촉의 법선 선택이 진동하는 버그를 수정했다. 현재 실행 중인 예전 프로세스에는
수정이 적용되지 않으므로 창을 닫고 run_live_demo.cmd로 다시 연다.
다시 계산 실패가 발생하면 마지막 정상 상태에서 정지하고 R/Reset으로 초기화한다.
실패 상태/입력/세부 잔차는 화면에 오류를 보내기 전에 work/live-failure.octostate와
work/live-failure.json에 저장한다. 증거/한계: issues/005-live-contact-ccd-normal-cycle.md.
