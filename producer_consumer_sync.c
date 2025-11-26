// 운영체제 프로젝트 2 - Producer/Consumer (동기화 적용 버전)
// 시나리오: 온라인 음식 배달 시스템의 "주문 접수 큐"
// - Producer: 고객이 앱에서 주문을 넣는 역할 (주문 생성 스레드)
// - Consumer: 주방에서 주문을 가져와 조리하는 역할 (요리사 스레드)
//
// 이 파일은 세마포어와 뮤텍스를 사용하여
// - 버퍼가 가득 찼을 때 생산자 대기
// - 버퍼가 비었을 때 소비자 대기
// - in/out/count/global_order_seq 를 상호배제(mutex)로 보호
// 하여 race condition 을 제거한다.

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <semaphore.h>

#define BUFFER_SIZE 5
#define NUM_PRODUCERS 3
#define NUM_CONSUMERS 2
#define ORDERS_PER_PRODUCER 20

typedef struct {
    int order_id;
    int customer_id;
} Order;

Order buffer[BUFFER_SIZE];
int in_index = 0;
int out_index = 0;
int count = 0;
int global_order_seq = 1;

// 세마포어 & 뮤텍스
sem_t empty_slots;   // 비어 있는 칸 개수
sem_t full_slots;    // 채워진 칸 개수
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // buffer/in/out/count/order_seq 보호

void random_sleep_short() {
    int us = rand() % 5000;
    usleep(us);
}

void *producer_thread(void *arg) {
    int producer_id = *(int *)arg;
    for (int i = 0; i < ORDERS_PER_PRODUCER; i++) {
        // 1. 비어 있는 칸 확보 (없으면 대기)
        sem_wait(&empty_slots);

        // 2. 공유 자원 배타적 접근
        pthread_mutex_lock(&mutex);

        int my_order_number = global_order_seq++;

        buffer[in_index].order_id = my_order_number;
        buffer[in_index].customer_id = producer_id;

        count++;
        printf("[P%d] (동기화) 주문 생성: order_id=%d 를 buffer[%d] 에 안전하게 삽입했습니다. "
               "(삽입 후 버퍼 개수 count=%d)\n",
               producer_id, my_order_number, in_index, count);

        in_index = (in_index + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&mutex);

        // 3. 채워진 칸 증가 알림
        sem_post(&full_slots);

        random_sleep_short();
    }

    printf("[P%d] Finished producing.\n", producer_id);
    return NULL;
}

void *consumer_thread(void *arg) {
    int consumer_id = *(int *)arg;

    // 총 소비해야 할 예상 주문 수
    int total_orders = NUM_PRODUCERS * ORDERS_PER_PRODUCER / NUM_CONSUMERS + 5;

    for (int i = 0; i < total_orders; i++) {
        // 1. 채워진 칸 확보 (없으면 대기)
        sem_wait(&full_slots);

        // 2. 공유 자원 배타적 접근
        pthread_mutex_lock(&mutex);

        Order o = buffer[out_index];
        count--;

        printf("[C%d] (동기화) 주문 소비: buffer[%d] 에서 order_id=%d (생산자 P%d) 를 안전하게 꺼냈습니다. "
               "(꺼낸 후 버퍼 개수 count=%d)\n",
               consumer_id, out_index, o.order_id, o.customer_id, count);

        out_index = (out_index + 1) % BUFFER_SIZE;

        pthread_mutex_unlock(&mutex);

        // 3. 비어 있는 칸 증가 알림
        sem_post(&empty_slots);

        random_sleep_short();
    }

    printf("[C%d] Finished consuming loop.\n", consumer_id);
    return NULL;
}

int main(void) {
    srand((unsigned int)time(NULL));

    // 세마포어 초기화
    // empty_slots: 처음에는 전부 비어 있으므로 BUFFER_SIZE
    // full_slots : 처음에는 채워진 칸 없음 -> 0
    if (sem_init(&empty_slots, 0, BUFFER_SIZE) != 0) {
        perror("sem_init empty_slots");
        return 1;
    }
    if (sem_init(&full_slots, 0, 0) != 0) {
        perror("sem_init full_slots");
        return 1;
    }

    pthread_t producers[NUM_PRODUCERS];
    pthread_t consumers[NUM_CONSUMERS];
    int producer_ids[NUM_PRODUCERS];
    int consumer_ids[NUM_CONSUMERS];

    printf("=== [SYNC] Producer/Consumer - Food Delivery Order Queue ===\n");
    printf("버퍼 크기(Buffer size)=%d, 생산자(Producer)=%d, 소비자(Consumer)=%d, "
           "각 Producer 주문 수(Orders per producer)=%d\n",
           BUFFER_SIZE, NUM_PRODUCERS, NUM_CONSUMERS, ORDERS_PER_PRODUCER);
    printf("※ 이 버전은 세마포어 + 뮤텍스를 이용해 동기화를 적용한 코드입니다.\n");
    printf("   - 버퍼가 가득 차면 Producer 가 자동으로 대기하고,\n");
    printf("   - 버퍼가 비면 Consumer 가 자동으로 대기합니다.\n");
    printf("   - 실행 로그에 WARNING/ERROR 가 없고, count 값이 항상 0~%d 사이에 머무는지 확인하세요.\n\n",
           BUFFER_SIZE);

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

    for (int i = 0; i < NUM_PRODUCERS; i++) {
        pthread_join(producers[i], NULL);
    }
    for (int i = 0; i < NUM_CONSUMERS; i++) {
        pthread_join(consumers[i], NULL);
    }

    printf("\n=== Program finished (SYNC). Final count=%d ===\n", count);

    sem_destroy(&empty_slots);
    sem_destroy(&full_slots);

    return 0;
}


