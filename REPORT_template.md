## 운영체제 프로젝트 2 - 보고서 템플릿

### 0. 프로그램 모델 (창의적 시나리오)

- **전체 시나리오 요약**
  - Producer/Consumer: 온라인 음식 배달 시스템의 주문 접수 큐
  - Reader/Writer: 모바일 뱅킹 시스템의 공유 은행 계좌 잔액
- **왜 이 시나리오를 선택했는지** (실제 서비스에서 동시성이 중요한 이유 등)

---

### 1. Producer/Consumer

#### 1-1. 동기화 없음 버전 개요

- **파일명**: `producer_consumer_nosync.c`
- **모델 설명**
  - 버퍼 크기: 5
  - Producer 수 / Consumer 수 / 각 Producer 의 주문 개수
  - 공유 변수: `buffer[]`, `in_index`, `out_index`, `count`, `global_order_seq`
  - 동기화 수단: **사용하지 않음 (의도적으로 race condition 유발)**

#### 1-2. 동작시험 결과 및 문제점 (race condition)

- **대표 실행 결과 (일부 인용/스크린샷 첨부)**  
  예시:
  - `WARNING: buffer empty but still consuming! count=0`
  - `*** ERROR: invalid count=-1 (out of range 0..5) ***`
- **문제 상황 설명**
  - Consumer 가 버퍼가 비어 있는데도 값을 소비하려는 상황
  - `count` 가 0 미만 또는 버퍼 크기 초과가 되는 상황
  - 생산/소비 순서가 뒤엉켜 버퍼 내용이 덮어쓰이거나, 같은 주문이 여러 번 소비되는 현상
- **원인 분석**
  - 여러 스레드가 `count`, `in_index`, `out_index` 를 동시에 읽고 수정하면서  
    read-modify-write 과정이 중간에 끊겨 다른 스레드에 의해 덮어써짐.
  - `usleep` 를 통해 context switching 을 유도하여 race condition 을 더욱 잘 관찰할 수 있음.

#### 1-3. 동기화 적용 버전 개요

- **파일명**: `producer_consumer_sync.c`
- **사용한 동기화 기법**
  - 세마포어: `empty_slots` (비어 있는 칸 개수), `full_slots` (채워진 칸 개수)
  - 뮤텍스: `mutex` (buffer/in/out/count/global_order_seq 보호)
- **알고리즘 요약**
  - Producer:
    - `sem_wait(empty_slots)` → `mutex lock` → 버퍼에 삽입 → `mutex unlock` → `sem_post(full_slots)`
  - Consumer:
    - `sem_wait(full_slots)` → `mutex lock` → 버퍼에서 제거 → `mutex unlock` → `sem_post(empty_slots)`

#### 1-4. 동기화 적용 후 동작시험 결과

- **대표 실행 결과 (일부)**  
  - `WARNING`/`ERROR` 메시지 없이 정상적인 생산/소비 로그
  - 종료 시: `=== Program finished (SYNC). Final count=0 ===`
- **문제 해결 확인**
  - `count` 가 항상 `0 ~ BUFFER_SIZE` 범위 안에 머무름
  - 버퍼가 가득 차면 producer 가 자동으로 대기하고, 비면 consumer 가 대기
  - 동일 주문이 여러 번 소비되거나, 비어 있는 버퍼에서 소비하는 문제가 사라짐

---

### 2. Reader/Writer

#### 2-1. 동기화 없음 버전 개요

- **파일명**: `reader_writer_nosync.c`
- **모델 설명**
  - 공유 변수: `balance` (초기값 1000)
  - Writer 스레드 수 / Reader 스레드 수
  - 각 Writer 의 연산 수 (`OPERATIONS_PER_WRITER`)
  - Writer 동작: 짝수 번째는 `+10`, 홀수 번째는 `-10` (합산하면 0이 되어야 함)
  - Reader 동작: 주기적으로 잔액을 찍어서 샘플링
- **이론적 기대값**
  - 각 writer 가 `+10`, `-10` 을 같은 횟수만큼 수행 → 최종 잔액 = 1000 이어야 함.

#### 2-2. 동작시험 결과 및 문제점 (race condition)

- **대표 실행 결과 (일부 인용/스크린샷)**  
  예시:
  - `=== Program finished (NO SYNC) ===`
  - `Expected balance=1000, Actual balance=940`
- **문제 상황 설명**
  - 최종 잔액이 1000이 아닌 값으로 끝나는 사례
  - Reader 가 중간에 어정쩡한 값(일관되지 않은 잔액)을 읽는 사례
- **원인 분석**
  - 여러 writer 가 동시에 `balance` 를 읽어서 local 변수에 복사한 후,  
    서로의 결과를 덮어써서 일부 업데이트가 사라지는 현상 (lost update).
  - 동기화가 없어서 read-modify-write가 원자적으로 수행되지 않음.

#### 2-3. 동기화 적용 버전 개요

- **파일명**: `reader_writer_sync.c`
- **사용한 동기화 기법 (Reader 우선 Readers-Writers)**
  - 정수 `readCount`: 현재 읽고 있는 reader 수
  - `readCount_mutex`: `readCount` 를 보호하는 뮤텍스
  - `resource_mutex`: 실제 공유 데이터(`balance_sync`) 를 보호하는 뮤텍스
- **알고리즘 요약**
  - Reader:
    - `readCount_mutex` 로 `readCount` 수정 보호
    - 첫 번째 reader 가 들어올 때 `resource_mutex` 를 lock
    - 마지막 reader 가 나갈 때 `resource_mutex` 를 unlock
  - Writer:
    - 항상 `resource_mutex` 를 lock 한 뒤 balance 를 읽고/쓰기

#### 2-4. 동기화 적용 후 동작시험 결과

- **대표 실행 결과 (일부)**  
  - `=== Program finished (SYNC) ===`
  - `Expected balance=1000, Actual balance=1000`
- **문제 해결 확인**
  - 여러 번 실행해도 기대 잔액과 실제 잔액이 항상 일치
  - Reader 가 동시에 여러 개 있어도, Writer 와의 충돌 없이 일관된 값만 관찰

---

### 3. 자가진단표

#### 3-1. 프로그램 모델

- [ ] 10점: Producer/Consumer와 Reader/Writer 모델을 기반으로 한 창의적인 프로그램 설계  
- [ ] 9점: Producer/Consumer와 Reader/Writer 모델을 기반으로 응용 프로그램 제작  
- [ ] 8점: 교재의 모델을 수정  
- [ ] 7점: 교재 모델의 최소한의 수정  
- [ ] 6점: 교재 모델을 그대로 구현  

> 선택한 단계: (예: 10점)  
> 선택한 이유/근거:  
> - 창의적 시나리오(배달 앱, 모바일 뱅킹) 적용 여부  
> - 단순 버퍼/정수 대신 구체적인 도메인 모델 사용 여부 등

#### 3-2. Producer/Consumer

- [ ] 10점: 동기화 기능 구현하여 문제점(race condition) 해결 확인  
- [ ] 9점: 동기화 기능 기본 동작 확인  
- [ ] 8점: 동기화 기능 구현  
- [ ] 7점: 동기화 제외한 프로그램의 문제점(race condition) 확인  
- [ ] 6점: 동기화를 제외한 프로그램 동작 확인  
- [ ] 5점: 동기화를 제외한 프로그램 동작 불가  

> 선택한 단계:  
> 주요 bug 및 프로그램의 문제점 / 개선 사항:  

#### 3-3. Reader/Writer

- [ ] 10점: 동기화 기능 구현하여 문제점(race condition) 해결 확인  
- [ ] 9점: 동기화 기능 기본 동작 확인  
- [ ] 8점: 동기화 기능 구현  
- [ ] 7점: 동기화 제외한 프로그램의 문제점(race condition) 확인  
- [ ] 6점: 동기화를 제외한 프로그램 동작 확인  
- [ ] 5점: 동기화를 제외한 프로그램 동작 불가  

> 선택한 단계:  
> 주요 bug 및 프로그램의 문제점 / 개선 사항:  

---

### 4. 토의 및 느낀점 (예시 가이드)

아래 질문들을 참고하여 자유 형식으로 작성하면 된다.

- **동기화 전** 프로그램을 처음 실행했을 때 어떤 현상이 가장 인상 깊었는가?
  - 예: count 가 음수가 되는 것, 최종 잔액이 계속 바뀌는 것 등
- 세마포/뮤텍스를 직접 사용해 보면서, 교재에서 배운 이론과 어떻게 연결되었는가?
- Readers-Writers 알고리즘(Reader 우선)을 직접 구현하면서 이해한 점 또는 어려웠던 점은 무엇인가?
- 실제 서비스(배달 앱, 모바일 뱅킹 등)에서 동기화가 잘못되면 어떤 문제가 발생할 수 있다고 생각하는가?
- 이번 과제를 통해 **운영체제의 동기화 개념**에 대해 새롭게 알게 된 점 또는 확실히 이해하게 된 점은 무엇인가?


