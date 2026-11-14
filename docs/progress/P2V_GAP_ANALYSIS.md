# 추가 체적 계약과 현재 코드의 차이

| 요구 | 현재 실제 상태 | 남은 작업/검증 |
|---|---|---|
| 몸통 물리 변형 | Body 6DOF 강체. 렌더 외형은 장식 | octo_fem, 혼합 압력·점성·양의 J, V16–V17 |
| 팔의 접촉 단면 | rod 반지름이 신장으로 바뀌는 원형 캡슐 | 잔차 cage 모드·접촉 납작해짐, 투영 멱등/영에너지, V18 |
| 빨판의 변형 피부 추적 | 강체/rod 앵커와 12점 조회 | FEM 경계 barycentric 패치, 법선·면적 갱신, V18–V19 |
| 몸통-8루트 반작용 | 강체 기준 양방향 AD 조립 | FEM 루트 접합, mass ledger, V19 |
| 피부 embedding | native rod 표면을 뷰어에서 재구성 | Tet/경계 바인딩, <2mm, 원본 해시·품질검사 |
| lumen과 부리 | 없음 | 닫힌 lumen, 유량·압력·제트 반작용, 부리 형상, V20–V21 |
| 곡면 수렴 | 고부하 tick1 실패, rollback | 원인 수정·동일 입력 회귀 |
| 일반 볼록/회전 CCD | 구/상자/캡슐 한정 | 일반 볼록 및 변형 경계 CCD |
| 실시간 연결 | **native stdio 실시간 상자/8팔 데모 검증 완료**, GDExtension은 stub | 체적 FEM 경계 스냅샷과 compiled extension/P0 검증은 남음 |
| 성능 | 기존 장면도 목표 미달 | 새 FEM 대표 장면 프로파일·최적화, 목표 달성 전 완료 금지 |

기존 파일을 지우지 않고 P1V.1→P1V.2→P2V.1→P2V.2→P2V.3→P2V.4 순으로
추가한다. 강체 비교 장면·기존 V01–V12는 유지한다. V16–V21은 아직 미구현이다.
물성/모델은 ASSUMED이며 BIO/사람 플레이 테스트는 별도 게이트다.

## 착수한 요소 모듈 (전체 P2와 구분)

core/include/octo/fem.hpp, core/src/fem.cpp: 독립 octo_fem 타깃.
사면체 기준 품질·양의 J, 등체적 Neo-Hookean mixed u-p saddle 에너지,
13개 국소 자유도 AD 힘/접선행렬, 압력 점프 후보 항, 내부 barycentric embedding,
요소 질량의 노드 배분을 추가했다. Release/Debug/ASan 요소 검사가 통과했다.
원래 full-world solver에는 아직 접합하지 않았다. 압력 안정화 계수의 세분화 검증,
외부 피부 점 투영, 점성 시간 적분 및 V16–V21은 아직 미완료다.
재현: ctest --preset release -R FEM --output-on-failure.
