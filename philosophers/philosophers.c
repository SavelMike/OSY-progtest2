#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <unistd.h>

pthread_mutex_t mutex;

#define THINKING -1
#define EATING 0
#define HUNGRY 1
#define PHILS 10
struct phil_data {
	int state;
	int id;
 	sem_t semaphor;
} phils[10];
	

int random_var(int min, int max) {
	return (int)( min + 1.0 * rand () / RAND_MAX * ( max - min ));

}

void think() {
	sleep(random_var(1, 10));
}

void eat() {
	sleep(random_var(1, 6));
}

int left_neighbour(int id) {
	return (id - 1) % PHILS;
}

int right_neighbour(int id) {
	return (id + 1) % PHILS;
}

void try_to_eat(int id) {
	if (phils[id].state == HUNGRY &&
	    phils[left_neighbour(id)].state != EATING &&
	    phils[right_neighbour(id)].state != EATING) {
		// Got forks
		phils[id].state = EATING;
		sem_post(&phils[id].semaphor);
	}
}

void take_forks(int id) {
	pthread_mutex_lock(&mutex);
	phils[id].state = HUNGRY;
	try_to_eat(id);
	pthread_mutex_unlock(&mutex);
	sem_wait(&phils[id].semaphor);
}


void put_forks(int id) {
	pthread_mutex_lock(&mutex);
	phils[id].state = THINKING;
	try_to_eat(left_neighbour(id));
	try_to_eat(right_neighbour(id));
	pthread_mutex_unlock(&mutex);
}


void* philosopher(void* arg) {
	struct phil_data* phil_data = (struct phil_data*)arg;
	while (1) {
		think();
		take_forks(phil_data->id);
		eat();
		put_forks(phil_data->id);
	}

	return NULL;
}

char* state_str(int state) {
	if (state == THINKING)
		return "thinking";
	if (state == EATING)
		return "eating";
	return "waiting";
}

void* print_thread(void* arg) {
	int i;
	
	while(1) {
		sleep(3);
		printf("*****\n");
		pthread_mutex_lock(&mutex);
		for (i = 0; i < 10; i++) {
			printf("phil %d : %s\n", phils[i].id,
			       state_str(phils[i].state));
		}
		pthread_mutex_unlock(&mutex);
	}
}

int main(void) {
	int i;
	int rc;
	pthread_t thread;
	pthread_t threads[10];

	pthread_create(&thread, NULL, print_thread, NULL);
	for (i = 0; i < 10; i++) {
		phils[i].state = THINKING;
		phils[i].id = i;
		sem_init(&phils[i].semaphor, 0, 0);
		rc = pthread_create(&thread, NULL, philosopher, &phils[i]);
		threads[i] = thread;
	}

	for (int i = 0; i < 10; i++) {
		pthread_join(threads[i], NULL);
	}

	return 0;
}
