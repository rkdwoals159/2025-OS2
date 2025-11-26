// 운영체제 프로젝트 2 - Producer/Consumer (동기화 없음 버전)
// 시나리오: 온라인 음식 배달 시스템의 "주문 접수 큐"
// - Producer: 고객이 앱에서 주문을 넣는 역할 (주문 생성 스레드)
// - Consumer: 주방에서 주문을 가져와 조리하는 역할 (요리사 스레드)
//
// 이 파일은 의도적으로 동기화를 전혀 사용하지 않아 race condition 이 발생하게 만든다.
// (in/out 인덱스와 count 를 보호하지 않음)

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 5       // 대기 주문 큐 크기
#define NUM_PRODUCERS 3     // 고객(주문 생성 스레드) 수
#define NUM_CONSUMERS 2     // 요리사(소비자 스레드) 수
#define ORDERS_PER_PRODUCER 20  // 각 고객이 넣는 주문 개수

typedef struct {
    int order_id;       // 주문 번호
    int customer_id;    // 고객 ID (어느 producer가 만든 주문인지)
} Order;

Order buffer[BUFFER_SIZE];
int in_index = 0;   // 다음에 쓸 위치
int out_index = 0;  // 다음에 뺄 위치
int count = 0;      // 버퍼 안의 주문 개수

int global_order_seq = 1;   // 전체 시스템에서 증가하는 주문 번호 (동기화 안 함!)

void random_sleep_short() {
    // 0 ~ 4ms 사이 랜덤 sleep -> context switching 가능성을 키움
    int us = rand() % 5000;
    usleep(us);
}

void *producer_thread(void *arg) {
    int producer_id = *(int *)arg;
    int i;
    for (i = 0; i < ORDERS_PER_PRODUCER; i++) {
        // 주문 생성 (여기서도 race condition 가능: global_order_seq)
        int my_order_number = global_order_seq;
        random_sleep_short();        // 중간에 끼어들 여지를 줌
        global_order_seq = global_order_seq + 1;

        // 버퍼가 가득 찼는지 확인 (동기화 없음)
        if (count == BUFFER_SIZE) {
            // 사실 여기서 기다리지 않고 그냥 덮어써버림 -> 버퍼 오버라이드
            printf("[P%d][경고] 버퍼가 가득 찼지만, 동기화가 없어 그대로 덮어쓰기를 시도합니다. "
                   "(현재 count=%d)\n",
                   producer_id, count);
        }

        // 버퍼에 주문 넣기
        buffer[in_index].order_id = my_order_number;
        buffer[in_index].customer_id = producer_id;

        printf("[P%d] 주문 생성: order_id=%d 를 buffer[%d] 위치에 넣습니다. "
               "(넣기 전 버퍼 개수 count=%d)\n",
               producer_id, my_order_number, in_index, count);

        random_sleep_short();    // 인덱스와 count를 업데이트하기 전 context switch 유도

        in_index = (in_index + 1) % BUFFER_SIZE;
        count++;    // 보호되지 않은 공유 변수 -> race condition 핵심

        // 조금 더 섞이도록 sleep
        random_sleep_short();
    }

    printf("[P%d] Finished producing.\n", producer_id);
    return NULL;
}

void *consumer_thread(void *arg) {
    int consumer_id = *(int *)arg;

    // 총 주문 수: NUM_PRODUCERS * ORDERS_PER_PRODUCER
    // 대략 그만큼 소비하도록 루프를 넉넉하게 돌린다.
    int i;
    for (i = 0; i < NUM_PRODUCERS * ORDERS_PER_PRODUCER; i++) {
        // 버퍼가 비어 있는지 확인 (동기화 없음)
        if (count == 0) {
            // 실제로는 기다려야 하지만, 여기서는 그냥 "유령 주문" 을 꺼내는 시도를 함
            printf("[C%d][경고] 버퍼가 비어 있는데도 소비를 시도합니다. "
                   "동기화가 없기 때문에 잘못된 데이터를 읽을 수 있습니다. (count=%d)\n",
                   consumer_id, count);
        }

        // 버퍼에서 주문 가져오기
        Order o = buffer[out_index];

        printf("[C%d] 주문 소비: buffer[%d] 에서 order_id=%d (생산자 P%d) 를 꺼냅니다. "
               "(꺼내기 전 버퍼 개수 count=%d)\n",
               consumer_id, out_index, o.order_id, o.customer_id, count);

        random_sleep_short();    // 인덱스와 count를 업데이트하기 전 context switch 유도

        out_index = (out_index + 1) % BUFFER_SIZE;
        count--;    // 보호되지 않은 공유 변수

        // 비정상적인 상황 감지용 출력
        if (count < 0 || count > BUFFER_SIZE) {
            printf("[C%d] *** ERROR: count 값이 유효 범위(0~%d)를 벗어났습니다. "
                   "현재 count=%d → race condition 으로 인한 심각한 불일치입니다. ***\n",
                   consumer_id, BUFFER_SIZE, count);
        }

        random_sleep_short();
    }

    printf("[C%d] Finished consuming loop.\n", consumer_id);
    return NULL;
}

int main(void) {
    srand((unsigned int)time(NULL));

    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];

    printf("=== [NO SYNC] Producer/Consumer - Food Delivery Order Queue ===\n");
    printf("버퍼 크기(Buffer size)=%d, 생산자(Producer)=%d, 소비자(Consumer)=%d, "
           "각 Producer 주문 수(Orders per producer)=%d\n",
           BUFFER_SIZE, NUM_PRODUCERS, NUM_CONSUMERS, ORDERS_PER_PRODUCER);
    printf("※ 이 버전은 '동기화 기능이 전혀 없는' 실험용 코드입니다.\n");
    printf("   - WARNING 메시지: 버퍼 상태를 잘못 판단하는 등 '이상 징후'를 의미\n");
    printf("   - *** ERROR 메시지: count 값이 음수/버퍼 크기 초과가 된 심각한 race condition 을 의미\n\n");

    // 스레드 생성
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        producer_ids[i] = i + 1;
        if (pthread_create(&producers[i], NULL, producer_thread, &producer_ids[i]) != 0) {
            perror("pthread_create producer");
            exit(1);
        }
    }

    for (int i = 0; i < NUM_CONSUMERS; i++) {
        consumer_ids[i] = i + 1;
        if (pthread_create(&consumers[i], NULL, consumer_thread, &consumer_ids[i]) != 0) {
            perror("pthread_create consumer");
            exit(1);
        }
    }

    // 스레드 종료 대기
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    printf("\n=== Program finished (NO SYNC). Final count=%d (may be inconsistent!) ===\n",
           count);
    return 0;
}


