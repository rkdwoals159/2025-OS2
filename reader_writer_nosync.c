// 운영체제 프로젝트 2 - Reader/Writer (동기화 없음 버전)
// 시나리오: 모바일 뱅킹 시스템의 "공유 은행 계좌 잔액"
// - Writer: ATM/모바일 송금 등 입출금 작업을 수행하는 스레드
// - Reader: 여러 사용자가 동시에 잔액을 조회하는 스레드
//
// 이 파일은 의도적으로 어떤 동기화도 사용하지 않아
// - 최종 잔액이 이론적으로 기대되는 값과 달라지거나
// - 잔액 조회 중간에 이상한(손실된) 값이 보이는
// race condition 을 보여준다.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_READERS 5
#define NUM_WRITERS 3
#define OPERATIONS_PER_WRITER 100000

int balance = 1000;  // 공유 은행 계좌 잔액 (보호 안 함)

void random_sleep_short() {
    int us = rand() % 2000;
    usleep(us);
}

void *writer_thread(void *arg) {
    int writer_id = *(int *)arg;

    for (int i = 0; i < OPERATIONS_PER_WRITER; i++) {
        // 단순 입금/출금 시뮬레이션: 짝수 번에는 +10, 홀수 번에는 -10
        int local = balance;   // 읽기
        random_sleep_short();  // 여기서 다른 쓰기/읽기가 끼어들 수 있음

        if (i % 2 == 0) {
            local += 10;
        } else {
            local -= 10;
        }

        random_sleep_short();  // 쓰기 직전 context switch 유도
        balance = local;       // 쓰기 (보호되지 않은 공유 변수)

        if (i % 20000 == 0) {
            printf("[W%d] operation %d, intermediate balance=%d\n",
                   writer_id, i, balance);
        }
    }

    printf("[W%d] Finished writing operations.\n", writer_id);
    return NULL;
}

void *reader_thread(void *arg) {
    int reader_id = *(int *)arg;

    // reader는 비교적 적은 횟수만 샘플링
    for (int i = 0; i < 1000; i++) {
        int snapshot = balance;  // 보호되지 않은 읽기
        random_sleep_short();

        if (i % 200 == 0) {
            printf("[R%d] read balance=%d (sample %d)\n",
                   reader_id, snapshot, i);
        }
    }

    printf("[R%d] Finished reading samples.\n", reader_id);
    return NULL;
}

int main(void) {
    srand((unsigned int)time(NULL));

    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    int reader_ids[NUM_READERS];
    int writer_ids[NUM_WRITERS];

    printf("=== Reader/Writer (NO SYNC) - Bank Account Balance ===\n");
    printf("Initial balance=%d, Readers=%d, Writers=%d\n\n",
           balance, NUM_READERS, NUM_WRITERS);

    // writer 스레드 생성
    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i + 1;
        if (pthread_create(&writers[i], NULL, writer_thread, &writer_ids[i]) != 0) {
            perror("pthread_create writer");
            exit(1);
        }
    }

    // reader 스레드 생성
    for (int i = 0; i < NUM_READERS; i++) {
        reader_ids[i] = i + 1;
        if (pthread_create(&readers[i], NULL, reader_thread, &reader_ids[i]) != 0) {
            perror("pthread_create reader");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }

    // 이론적인 최종 잔액 계산
    // 각 writer는 +10, -10 을 같은 횟수만큼 수행하므로
    // 순수하게 계산하면 최종 balance 는 초기값과 같아야 한다.
    int expected_balance = 1000;

    printf("\n=== Program finished (NO SYNC) ===\n");
    printf("Expected balance=%d, Actual balance=%d\n",
           expected_balance, balance);
    printf("-> 두 값이 다르면 race condition 으로 인한 데이터 손실/중복 발생을 의미.\n");

    return 0;
}


