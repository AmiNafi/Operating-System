// #include<stdio.h>
#include <bits/stdc++.h>

#include<pthread.h>
// #include<stdlib.h>
#include<unistd.h>
#include<semaphore.h>
#include <random>
#include <chrono>
#define INIT 0
#define WAITING 2
#define PRINTING 3
#define PRINTED 4
using namespace std;
const int MAX_STUDENTS = 100, MAX_PRINTER = 5, MAX_STUFFS = 3;
sem_t sema[MAX_STUDENTS], binding_sema, db_sema;
int state[MAX_STUDENTS], isPrinterAvailable[MAX_PRINTER];
int total_printer, group_size, printing_time, total_students;
int binding_time, rw_time, threadIDs[MAX_STUDENTS];
int stuffIDs[MAX_STUFFS], rc = 0, entry_count = 0;
pthread_mutex_t mtx, outlock, entry_lock, rwlock;
pthread_t threads[MAX_STUDENTS], stuff[MAX_STUFFS];
auto start_time = std::chrono::high_resolution_clock::now();
// auto start_time = std::chrono::system_clock::now();
int gen_random(double mean) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::poisson_distribution<int> poisson(mean);

    // std::random_device rd;
    // std::mt19937 gen(rd());
    // std::poisson_distribution < int > poisson(mean);
    return (poisson(gen));
}

int getGroupId(int student_id) {
    return (student_id + group_size - 1) / group_size;
}
int getPrinterId(int student_id) {
    return student_id % total_printer + 1;
}

void tryPrint(int student_id) {
    if (state[student_id] == WAITING && isPrinterAvailable[getPrinterId(student_id)]) {
        state[student_id] = PRINTING;        
        isPrinterAvailable[getPrinterId(student_id)] = 0;
        sem_post(&sema[student_id]);
    }
}
void take_printer(int student_id) {
    pthread_mutex_lock(&mtx);
    state[student_id] = WAITING;
    tryPrint(student_id);
    pthread_mutex_unlock(&mtx);
    sem_wait(&sema[student_id]);
}

void leave_printer(int student_id) {
    pthread_mutex_lock(&mtx);
    state[student_id] = PRINTED;
    isPrinterAvailable[getPrinterId(student_id)] = 1;
    for (int i = 1; i <= getGroupId(student_id) * group_size; i++) {
        if (getGroupId(i) == getGroupId(student_id) && getPrinterId(i) == getPrinterId(student_id)) tryPrint(i);
    }
    for (int i = 1; i <= total_students; i++) {
        if (getGroupId(i) != getGroupId(student_id) && getPrinterId(i) == getPrinterId(student_id)) tryPrint(i);
    }
    pthread_mutex_unlock(&mtx);
}
void read_entry(int id, int &complete) {
    complete = 0;
    sleep(rw_time);
    pthread_mutex_lock(&outlock);
    auto current_time = std::chrono::high_resolution_clock::now();
    time_t time = (current_time - start_time).count() / 1e9;
    printf("Staff %d has started reading the entry book at time %ld. No. of submission = %d\n", id, time, entry_count);
    if (entry_count == total_students / group_size) complete = 1;
    pthread_mutex_unlock(&outlock);
}
void write_entry(int group_id) {
    sem_wait(&db_sema);
    sleep(rw_time);
    pthread_mutex_lock(&entry_lock);
    entry_count++;
    pthread_mutex_unlock(&entry_lock);
    pthread_mutex_lock(&outlock);
    auto current_time = std::chrono::high_resolution_clock::now();
    time_t time = (current_time - start_time).count() / 1e9;
    printf("Group %d has submitted the report at time %ld\n", group_id, time);
    pthread_mutex_unlock(&outlock);
    sem_post(&db_sema);
}
void * philosopher(void * arg) {
    sleep(gen_random(5));

    int student_id = (*(int*)arg);
    
    pthread_mutex_lock(&outlock);
    auto current_time = std::chrono::high_resolution_clock::now();
    time_t time = (current_time - start_time).count() / 1e9;
    printf("Student %d has arrived at the print station at time %ld\n", student_id, time);
    pthread_mutex_unlock(&outlock);

    take_printer(student_id);
    sleep(printing_time);
    pthread_mutex_lock(&outlock);
    current_time = std::chrono::high_resolution_clock::now();
    time = (current_time - start_time).count() / 1e9;
    printf("Student %d has finished printing at time %ld\n", student_id, time);
    pthread_mutex_unlock(&outlock);
    leave_printer(student_id);
    if (student_id % group_size == 0) {
        for (int i = 1; i <= total_students; i++) {
            if (i != student_id && getGroupId(i) == getGroupId(student_id)) {
                pthread_join(threads[i], NULL);
            }
        }
        pthread_mutex_lock(&outlock);
        auto current_time = std::chrono::high_resolution_clock::now();
        time_t time = (current_time - start_time).count() / 1e9;
        printf("Group %d has finished printing at time %ld\n", getGroupId(student_id), time);
        pthread_mutex_unlock(&outlock);
        sem_wait(&binding_sema);
        pthread_mutex_lock(&outlock);
        current_time = std::chrono::high_resolution_clock::now();
        time = (current_time - start_time).count() / 1e9;
        printf("Group %d has started binding at time %ld\n", getGroupId(student_id), time);
        pthread_mutex_unlock(&outlock);
        sleep(binding_time);
        pthread_mutex_lock(&outlock);
        current_time = std::chrono::high_resolution_clock::now();
        time = (current_time - start_time).count() / 1e9;
        printf("Group %d has finished binding at time %ld\n", getGroupId(student_id), time);
        pthread_mutex_unlock(&outlock);
        sem_post(&binding_sema);
        write_entry(getGroupId(student_id));
    }
    return 0;
}

void * reader(void * arg) {
    int id = (*(int*)arg);
    int complete = 0;
    while (true) {
        sleep(gen_random(5.0));
        pthread_mutex_lock(&rwlock);
        rc = rc + 1;
        if (rc == 1) sem_wait(&db_sema);
        pthread_mutex_unlock(&rwlock);
        read_entry(id, complete);
        
        pthread_mutex_lock(&rwlock);
        rc = rc - 1;
        if (rc == 0) sem_post(&db_sema);
        pthread_mutex_unlock(&rwlock);
        if (complete) break;
    }
    return 0;
}
void init() {
    total_printer = 4;
    pthread_mutex_init(&mtx, NULL);
    pthread_mutex_init(&outlock, NULL);
    pthread_mutex_init(&entry_lock, NULL);
    pthread_mutex_init(&rwlock, NULL);
    for (int i = 1; i <= total_students; i++) {
        sem_init(&sema[i], 0, 0);
    }
    sem_init(&binding_sema, 0, 2);
    sem_init(&db_sema, 0, 1);
    for (int i = 1; i < 5; i++) isPrinterAvailable[i] = 1;
}

int main(void)
{	
    freopen("input.txt", "r", stdin);
    freopen("output.txt", "w", stdout);
    scanf("%d%d%d%d%d", &total_students, &group_size, &printing_time, &binding_time, &rw_time);
    init();
    for (int i = 1; i <= total_students; i++) {
        // pthread_mutex_lock(&outlock);
        // // printf("here %d\n", i);
        // pthread_mutex_unlock(&outlock);
        threadIDs[i] = i;
        pthread_create(&threads[i], NULL, philosopher, threadIDs + i);
    }
    for (int i = 1; i <= 2; i++) {
        stuffIDs[i] = i;
        pthread_create(&stuff[i], NULL, reader, stuffIDs + i);
    }

    for (int i = group_size; i <= total_students; i += group_size) {
        pthread_join(threads[i], NULL);
    }
    for (int i = 1; i <= 2; i++) {
        pthread_join(stuff[i], NULL);
    }
	return 0;
}
