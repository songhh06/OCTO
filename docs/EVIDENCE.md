# 근거·가정·검증 장부

## 2026-11-14 구현 증거 추가

독립 C++20 구현과 해석/수치 검증 결과는 progress/P1_P2_REPORT.md,
실제 사용한 식·단위·좌표·구현 차이는 PHYSICS_MODEL.md,
미검증 상수와 허용 범위는 ../profiles/dynamics-model.json에 기록했다.
해당 프로파일은 컴파일 기본값의 감사용 명세이며 런타임 로더가 아니다.
E1–E4 원문의 개별 쪽/수식 번호 대조 및 생체 물성 확인은 여전히 미완료다.
보 이론·압력 단위·운동량·항력 해석 시험은 공학 모델 구현 검증이며 BIO 근거가 아니다.

버전 1.0 · 조사일 2026-11-14. 논문 제목/DOI와 Godot 최신 안정 릴리스는 출판·공식 릴리스 메타데이터로 확인했다. 아래 문헌이 게임에 적힌 모든 구성방정식과 수치를 직접 검증한다는 뜻은 아니다. `GAME_SPEC.md`에서 `[모델]`, `[초기값]`으로 표시한 것은 개발 가설이다.

## 1. 확인한 출처

| ID | 출처 | 이 문서에서 사용할 수 있는 근거 | 사용 한계 |
|---|---|---|---|
| E1 | Gazzola et al., “Forward and inverse problems in the mechanics of soft filaments,” *Royal Society Open Science* 5, 171628 (2018), [DOI](https://doi.org/10.1098/rsos.171628) | Cosserat rod의 위치·단면 회전·전단·굽힘·비틀림·수치 해법의 일반 틀 | 문어 팔의 물성값이나 게임 솔버 선택을 직접 확정하지 않음 |
| E2 | Tekinalp et al., “Topology, dynamics, and control of a muscle-architected soft arm,” *PNAS* (2024), [DOI](https://doi.org/10.1073/pnas.2318769121) | 근육 배치와 연속체 팔의 능동 제어를 연결하는 연구 사례 | 본 문서의 7채널 분할, `f_l/f_v`, 재료값은 별도 게임 모델 |
| E3 | “Classification and Evaluation of Octopus-Inspired Suction Cups for Soft Continuum Robots,” *Advanced Science* (2024), [DOI](https://doi.org/10.1002/advs.202400806), [공개 원문](https://pmc.ncbi.nlm.nih.gov/articles/PMC11321698/) | 문어 모사 빨판의 구조·기능·로봇 평가에 대한 비교 자료 | 자연 문어 빨판의 압력-누출-박리 수치 상수를 제공한다고 가정하지 않음 |
| E4 | Yekutieli et al., “Dynamic Model of the Octopus Arm. I. Biomechanics of the Octopus Reaching Movement,” *Journal of Neurophysiology* (2005), [DOI](https://doi.org/10.1152/jn.00684.2004) | 문어 팔의 수중 도달 운동에 동역학·유체 효과가 중요하다는 연구 맥락 | 국소 항력 계수를 모든 종·동작·스케일에 그대로 대입하지 않음 |
| E5 | [Godot 4.7.2 stable 공식 릴리스](https://github.com/godotengine/godot/releases/tag/4.7.2-stable) | 2026-11-14 현재 존재하는 안정 배포 버전 | 실제 개발 PC의 바이너리 무결성·플랫폼 호환성은 별도 확인 |
| E6 | [Godot GDExtension 공식 문서](https://docs.godotengine.org/en/stable/tutorials/scripting/gdextension/what_is_gdextension.html), [godot-cpp 저장소](https://github.com/godotengine/godot-cpp) | C++ 코드를 엔진 외부 확장으로 연결하는 공식 경로 | 코어-엔진 결합 솔버의 성능·정확성을 보증하지 않음 |

원문 수식과 게임 수식의 대응은 코드의 모듈 문서에 페이지·식 번호로 추가한다. 특히 E3의 로봇 빨판 연구를 자연 문어의 실측치로 혼동하지 않는다. DOI·공식 릴리스 링크가 변경되거나 철회되면 문서와 `toolchain.lock.json`을 갱신한다.

## 2. 물리 모델별 근거 등급

| 항목 | 등급 | 현재 상태 | 다음 검증 |
|---|---|---|---|
| 3D Cosserat 운동학과 운동량 보존 | 일반 역학 근거 | 방정식 선택, 구현 전 | 보 이론·힘/토크 반작용·수렴 V01–V02,V09 |
| 문어 팔의 거의 비압축성·근육 배열 | 생체 구조 근거 + 1D 근사 | 7채널·원형 단면은 설계 선택 | 부피 V04, 실제 형태 영상 비교 BIO |
| `σmax`, 조직 E/G, 점성, 활성 시간 | **미검증 초기값** | 종·크기·온도 불명 | 문헌 범위 조사, 실험 자료와 파라미터 추정 |
| 빨판 `C_eff`, `G_seal`, 12샘플 박리, `G_c` | **제안 모델** | 압력·누출·에너지 단위 검증 필요 | V06–V08, 실측 압력·하중·박리 비교 BIO |
| 표면별 유효 마찰과 밀봉 가능성 | **미검증 초기값** | 재질 이름만으로 실제 값을 알 수 없음 | 표면 조도·젖음성·하중 조건 실측 |
| 국소 방향별 항력·부력 | 일반 유체 역학 근사 | 완전 CFD/유동 반작용 없음 | 해석 종단 속도 V11, 수중 영상 비교 BIO |
| 결합 implicit solver·허용 오차 | **공학 설계 선택** | 성능과 수렴 미측정 | V09–V13, 수치 차분 Jacobian |
| 24요소/팔, 16빨판/팔, 120 Hz | **성능 시작점** | 대표 하드웨어에서 미측정 | 수렴/성능 곡선, 12·24·32요소 비교 |
| 조작감·카메라·HUD·음향 | **제품 가설** | 실기 플레이 전 | P3/P4 사용자 테스트 |

## 3. 생체 비교 프로토콜(BIO 게이트)

실제 문어와의 정량적 일치를 주장하려면 최소한 종, 크기, 수온·수심, 팔 길이/지름, 영상 촬영 속도·시점, 바닥/벽 재질, 하중·표면 조도를 기록한다. 공개 영상만 있으면 길이·시간 보정의 불확실성을 표시한다. 비교 지표: 팔 끝 3D 궤적(가능하면 다중 카메라), 굽힘파 전파 속도, 축 신장 비, 몸통 속도, 빨판 유지력/박리 순서·하중. 각 지표는 평균뿐 아니라 변동 범위와 측정 오차를 제시한다. 최적화에 사용한 자료와 검증 자료를 분리한다. 사람 전문가 또는 실제 측정 데이터 없이 `BIO 통과`라고 쓰지 않는다.

## 4. 출처 추가·값 수정 절차

새 논문/데이터마다 DOI 또는 영구 URL, 저자·연도, 측정 종/조건, 사용한 수식·표·그림 번호, 허용 사용 범위, 도출한 파라미터와 오차를 기록한다. 값이 바뀌면 이전값·새값·단위·재현 장면·V 테스트 결과·커밋을 함께 남긴다. 논문에 없는 수치는 `ASSUMED`나 `TUNED_FOR_GAMEPLAY`로 표시한다. 자료가 상충하면 한쪽을 임의 선택하지 말고 조건 차이를 설명한 뒤 감도 분석을 수행한다.


## 체적 모델 후보 (모두 ASSUMED, BIO 미검증)

등체적 Neo-Hookean, mixed 변위-압력 안정화, 점성, rod-cage 질량 가중 투영,
lumen 압력/유량, embedding과 부리 최소구경은 추가 명세의 후보 모델이다.
문어 종·온도·조직 실측 물성 및 추정 오차는 아직 없다. 구현의 수치 검증과
실제 생체 타당성 승인을 구분한다. 조직 부피와 lumen 유체 체적은 따로 검증한다.
