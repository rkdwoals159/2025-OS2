// 운영체제 프로젝트 2 - Reader/Writer (동기화 적용 버전)
// 시나리오: 모바일 뱅킹 시스템의 "공유 은행 계좌 잔액"
// - Writer: ATM/모바일 송금 등 입출금 작업을 수행하는 스레드
// - Reader: 여러 사용자가 동시에 잔액을 조회하는 스레드
//
// 이 파일은 고전적인 Readers-Writers 문제의 해법(Reader 우선 버전)을 사용한다.
// - readCount: 현재 읽고 있는 reader 수
// - readCount 보호용 mutex
// - 공유 자원(balance)을 보호하는 resource_mutex
//
// 여러 reader 가 동시에 balance 를 읽을 수 있지만,
// writer 가 쓸 때는 어떤 reader 도 읽을 수 없다.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define NUM_READERS 5
#define NUM_WRITERS 3
#define OPERATIONS_PER_WRITER 10000

int balance_sync = 1000;
int readCount = 0;

pthread_mutex_t readCount_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t resource_mutex = PTHREAD_MUTEX_INITIALIZER;

void random_sleep_short() {
    int us = rand() % 2000;
    usleep(us);
}

void *writer_thread(void *arg) {
    int writer_id = *(int *)arg;

    for (int i = 0; i < OPERATIONS_PER_WRITER; i++) {
        // Writer는 공유 자원 전체에 대한 배타적 접근 필요
        pthread_mutex_lock(&resource_mutex);

        int local = balance_sync;
        random_sleep_short();

        if (i % 2 == 0) {
            local += 10;   // 입금
        } else {
            local -= 10;   // 출금
        }

        random_sleep_short();
        balance_sync = local;

        if (i % 20000 == 0) {
            printf("[W%d][동기화] %d번째 입출금 후 잔액=%d "
                   "(resource_mutex 로 보호되어 다른 Writer 와의 race condition 이 없음)\n",
                   writer_id, i, balance_sync);
        }

        pthread_mutex_unlock(&resource_mutex);

        random_sleep_short();
    }

    printf("[W%d] Finished writing operations.\n", writer_id);
    return NULL;
}

void *reader_thread(void *arg) {
    int reader_id = *(int *)arg;

    for (int i = 0; i < 1000; i++) {
        // 1. readCount 증가 및 첫 번째 reader 처리
        pthread_mutex_lock(&readCount_mutex);
        readCount++;
        if (readCount == 1) {
            // 첫 reader 가 들어올 때 resource 를 lock (writer 차단)
            pthread_mutex_lock(&resource_mutex);
        }
        pthread_mutex_unlock(&readCount_mutex);

        // 2. 공유 자원 읽기 (여러 reader 가 동시에 가능)
        int snapshot = balance_sync;
        random_sleep_short();

        if (i % 200 == 0) {
            printf("[R%d][동기화] 샘플 %d: 잔액 읽기 balance=%d (현재 동시에 읽는 reader 수 readCount=%d)\n",
                   reader_id, i, snapshot, readCount);
        }

        // 3. readCount 감소 및 마지막 reader 처리
        pthread_mutex_lock(&readCount_mutex);
        readCount--;
        if (readCount == 0) {
            // 마지막 reader 가 나갈 때 resource unlock (writer 허용)
            pthread_mutex_unlock(&resource_mutex);
        }
        pthread_mutex_unlock(&readCount_mutex);

        random_sleep_short();
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

    printf("=== [SYNC] Reader/Writer - Bank Account Balance ===\n");
    printf("초기 잔액(Initial balance)=%d, Reader 수=%d, Writer 수=%d\n",
           balance_sync, NUM_READERS, NUM_WRITERS);

    for (int i = 0; i < NUM_WRITERS; i++) {
        writer_ids[i] = i + 1;
        if (pthread_create(&writers[i], NULL, writer_thread, &writer_ids[i]) != 0) {
            perror("pthread_create writer");
            exit(1);
        }
    }

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

    int expected_balance = 1000;

    printf("\n=== Program finished (SYNC) ===\n");
    printf("Expected balance=%d, Actual balance=%d\n",
           expected_balance, balance_sync);
    return 0;
}


