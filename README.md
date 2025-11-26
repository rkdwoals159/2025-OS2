## 운영체제 프로젝트 2 - 동기화 문제

### 1. 개요

- **주제**: Producer/Consumer 및 Reader/Writer 모델에서의 race condition 실험과 동기화(synchronization)를 이용한 해결
- **언어 / 환경**: C, POSIX 스레드(`pthread`), (제출용 기준) Linux 환경
- **창의적 시나리오**
  - **Producer/Consumer**: 온라인 음식 배달 시스템의 **주문 접수 큐**
    - Producer: 고객이 앱에서 주문을 넣는 스레드
    - Consumer: 주방에서 주문을 가져와 조리하는 요리사 스레드
  - **Reader/Writer**: 모바일 뱅킹 시스템의 **공유 은행 계좌 잔액**
    - Writer: ATM/모바일 송금 등 입출금 작업 스레드
    - Reader: 여러 사용자가 동시에 잔액을 조회하는 스레드

---

### 2. 파일 구성

- **Producer/Consumer**

  - `producer_consumer_nosync.c`
    - 동기화 없이 공유 버퍼(in, out, count)를 사용하는 버전
    - 의도적으로 race condition 발생 (음수 count, 버퍼 초과 등)
  - `producer_consumer_sync.c`
    - 세마포어(`sem_t`)와 뮤텍스(`pthread_mutex_t`)를 사용하여  
      bounded buffer 문제를 해결한 버전

- **Reader/Writer**

  - `reader_writer_nosync.c`
    - 공유 변수 `balance` 에 대해 어떤 보호도 하지 않는 버전
    - 최종 잔액이 이론값과 달라지는 현상으로 race condition 관찰
  - `reader_writer_sync.c`
    - 고전적인 Readers-Writers 문제(Reader 우선)를 구현한 버전
    - `readCount`, `readCount_mutex`, `resource_mutex` 를 사용

- **실행 파일 (예시 이름)**
  - `pc_nosync`, `pc_sync`, `rw_nosync`, `rw_sync`

---

### 3. 컴파일 방법

Linux 환경에서 다음과 같이 컴파일할 수 있다.

```bash
gcc -Wall -Wextra -pthread producer_consumer_nosync.c -o pc_nosync
gcc -Wall -Wextra -pthread producer_consumer_sync.c   -o pc_sync

gcc -Wall -Wextra -pthread reader_writer_nosync.c     -o rw_nosync
gcc -Wall -Wextra -pthread reader_writer_sync.c       -o rw_sync
```

> 주의: macOS에서 `sem_init` 가 deprecated 이거나 동작하지 않을 수 있으므로,  
> 제출/채점용은 **Linux** 머신(실습실 서버 등)에서 실행하는 것을 전제로 한다.

---

### 4. 실행 방법 및 관찰 포인트

#### 4.1 Producer/Consumer (동기화 없음)

```bash
./pc_nosync
```

  - 출력 중 다음과 같은 메시지가 나타날 수 있다.
    - `WARNING: buffer empty but still consuming! count=0`
    - `WARNING: buffer full but still inserting! count=5`
    - `*** ERROR: invalid count=-1 (out of range 0..5) ***`
  - 공유 변수 `count` 가 **음수 또는 버퍼 크기보다 큰 값**이 되는 것을 통해  
    동기화가 없을 때 **race condition** 이 발생함을 확인할 수 있다.

#### 4.2 Producer/Consumer (동기화 적용)

```bash
./pc_sync
```

  - `WARNING` 나 `*** ERROR` 메시지가 나타나지 않는다.
  - `count` 값이 항상 `0 ~ BUFFER_SIZE` 범위 안에 머문다.
  - 프로그램 종료 시 출력:
    - `=== Program finished (SYNC). Final count=0 ===`
  - 세마포어(`empty_slots`, `full_slots`)와 뮤텍스(`mutex`)를 통해
    - 버퍼가 **가득 찬 경우 producer는 대기**
    - 버퍼가 **비어 있는 경우 consumer는 대기**
    - in/out/count/global_order_seq 에 **상호배제(mutex)** 가 적용됨을 확인할 수 있다.

---

#### 4.3 Reader/Writer (동기화 없음)

```bash
./rw_nosync
```

  - 프로그램 종료 시:
    - `Expected balance=1000, Actual balance=xxx` 출력
  - 이론적으로는 각 writer 가 `+10`, `-10` 을 같은 횟수 수행하므로  
    최종 잔액은 항상 1000 이어야 한다.
  - 그러나 동기화가 없으면 **Actual balance != 1000** 인 경우가 자주 발생하며,  
    이는 동시에 여러 writer 가 `balance` 를 읽고/쓰는 과정에서  
    **업데이트가 덮어써지거나 손실되는 race condition** 이 발생했음을 의미한다.

#### 4.4 Reader/Writer (동기화 적용)

```bash
./rw_sync
```

  - 프로그램 종료 시:
    - `Expected balance=1000, Actual balance=1000`
  - `readCount` 가 0에서 1이 될 때, 마지막 reader 가 나갈 때만  
    `resource_mutex` 를 lock/unlock 하는 패턴을 통해
    - 여러 reader 가 동시에 읽을 수 있지만,
    - writer 가 작업하는 동안에는 reader 가 접근하지 못하도록 보장한다.
  - 결과적으로 **기대한 잔액과 실제 잔액이 항상 일치**하며,  
    동기화를 통해 race condition 이 제거되었음을 확인할 수 있다.

---
