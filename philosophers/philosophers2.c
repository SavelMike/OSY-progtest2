#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t mutex;

#define PHILS 10

#define AVAILABLE 0
#define USED 1
struct fork_data {
	int state;
	int phil_id;
} forks[PHILS];

struct phil_data {
	pthread_cond_t cond_var;
} phils[PHILS];

int random_var(int min, int max) {
	return (int)( min + 1.0 * rand () / RAND_MAX * ( max - min ));
}

void think() {
	sleep(random_var(1, 10));
}

void eat() {
	sleep(random_var(1, 6));
}

int left_fork(int id) {
	return id % PHILS;
}

int right_fork(int id) {
	return (id + 1) % PHILS;
}

int both_forks_available(int id) {
	if (forks[left_fork(id)].state == AVAILABLE &&
	    forks[right_fork(id)].state == AVAILABLE) {
		return 1;
	}

	return 0;
}

void take_forks(int id) {
	pthread_mutex_lock(&mutex);
	while (!both_forks_available(id)) {
		pthread_cond_wait(&phils[id].cond_var, &mutex);
	}
	forks[left_fork(id)].state = USED;
	forks[right_fork(id)].state = USED;
	forks[left_fork(id)].phil_id = id;
	forks[right_fork(id)].phil_id = id;
	pthread_mutex_unlock(&mutex);
}

void put_forks(int id) {
	pthread_mutex_lock(&mutex);
	forks[left_fork(id)].state = AVAILABLE;
	forks[right_fork(id)].state = AVAILABLE;
	pthread_cond_signal(&phils[left_fork(id)].cond_var);
	pthread_cond_signal(&phils[right_fork(id)].cond_var);
	pthread_mutex_unlock(&mutex);
}


void* philosopher(void* arg) {
	int id = (int)arg;
	while (1) {
		think();
		take_forks(id);
		eat();
		put_forks(id);
	}

	return NULL;
}

void* print_thread(void* arg) {
	int i;
	
	while(1) {
		sleep(3);
		printf("*****\n");
		pthread_mutex_lock(&mutex);
		for (i = 0; i < PHILS; i++) {
			if (forks[i].state == USED &&
			    forks[i].phil_id == i) {
				printf("fork state %d eating\n", i);
			}
		}
		pthread_mutex_unlock(&mutex);
	}
}


int main(void) {
	int i;
	pthread_t thread;
	pthread_t threads[PHILS];

	pthread_create(&thread, NULL, print_thread, NULL);
	for (i = 0; i < PHILS; i++) {
		forks[i].state = AVAILABLE;
		pthread_cond_init(&phils[i].cond_var, NULL);
		pthread_create(&thread, NULL, philosopher, (void*)i);
		threads[i] = thread;
	}

	for (i = 0; i < PHILS; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
