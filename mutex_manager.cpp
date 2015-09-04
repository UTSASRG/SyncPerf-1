#include<stdlib.h>
#include<string.h>
#include<list>
#include<map>
#include<vector>
#include<iostream>
#include<fstream>
#include "mutex_manager.h"
//#include "libfuncs.h"
#include "xdefines.h"

//typedef std::map< std::vector<long>, std::vector<my_mutex_t*> > hash_map_t;
//hash_map_t mutex_map; // hash map for call stack - mutex
pthread_mutex_t mutex_map_lock=PTHREAD_MUTEX_INITIALIZER;

std::list<my_mutex_t*>mutex_list;
pthread_mutex_t mutex_list_lock=PTHREAD_MUTEX_INITIALIZER; // global lock 

my_mutex_t* create_mutex( pthread_mutex_t *mutex )
{
    //printf("create my mutex\n");
    my_mutex_t *new_mutex =(my_mutex_t*)malloc(sizeof(my_mutex_t));
    new_mutex->count = 0;
    new_mutex->mutex = *mutex;
		new_mutex->stack_count = 0;
		memset(new_mutex->data, 0, sizeof(mutex_meta_t)*MAX_NUM_STACKS);
#if 0 // TODO: do i really need to initialize array  with 0??
		memset(new_mutex->futex_wait, 0, sizeof(WAIT_TIME_TYPE)*M_MAX_THREADS);
		memset(new_mutex->cond_futex_wait, 0, sizeof(WAIT_TIME_TYPE)*M_MAX_THREADS);
		memset(new_mutex->trylock_wait_time, 0, sizeof(WAIT_TIME_TYPE)*M_MAX_THREADS);
	  memset(new_mutex->trylock_flag, 0, sizeof(int)*M_MAX_THREADS);
		memset(new_mutex->trylock_fail_count, 0, sizeof(int)*M_MAX_THREADS);
//#else
		new_mutex->futex_wait[0] = 0;
		new_mutex->cond_futex_wait[0] = 0;
		new_mutex->trylock_wait_time[0] = 0;
		new_mutex->trylock_flag[0] = 0;
		new_mutex->trylock_fail_count[0] = 0;
#endif
		append(new_mutex);
    return new_mutex;
}

int is_my_mutex(void *mutex)
{
    void **tmp;
    tmp = mutex;

    if( *tmp != NULL)
        return 1;
    else 
        return 0;
}

void* get_mutex( void *mutex )
{
    void **tmp;
    tmp = mutex;

    assert(*tmp != NULL);
    return *tmp;    
    
}

#if 0
pthread_mutex_t* get_orig_mutex( my_mutex *m ) 
{
    return &m->mutex;
}
#endif

int setSyncEntry( void* syncvar, void* realvar) {
    int ret = 0;
    unsigned long* target = (unsigned long*)syncvar;
    unsigned long expected = *(unsigned long*)target;
    
    if( !is_my_mutex(target) ) //double check
    {
        if(__sync_bool_compare_and_swap(target, expected, (unsigned long)realvar)) 
        {
            //printf("new mutex \n");
            ret = 1;
        }
        else {
            free(realvar);
        } 
    }
    return ret;
}

void add_call_stack( my_mutex_t *mutex, long call_stack[] ){
	int i=0;
	mutex->stack_count++;
	while(call_stack[i] != 0 ) {
		mutex->stacks[mutex->stack_count-1][i] = call_stack[i];
		i++;
	}
	mutex->stacks[mutex->stack_count-1][i] = 0;
}

//return 1 if match, otherwise 0
int comp_stack( long s1[], long s2[] ){
	int found = 1;
	int idx = 0;
	while( s1[idx] != 0 && s2[idx] !=0 ){
		if(s1[idx] != s2[idx] ) {
			found = 0;
			break;
		}
		idx++;
	}

	if( found != 0 ) {
		if(s1[idx] != 0 || s2[idx] != 0 ){
			found = 0;
		}
	}

	return found;
}


//TODO: use a hashtable
/*
 * reutrn matching call stack data
 */

mutex_meta_t* get_mutex_meta( my_mutex_t *mutex, long call_stack[] ){

	//compare  all the call stacks and get the index for data[] in mutex_t
	int i;
	int found = 0;
	WRAP(pthread_mutex_lock)(&mutex_map_lock);  
	if(mutex->stack_count == 0 ){ // first one
		add_call_stack(mutex, call_stack);
		WRAP(pthread_mutex_unlock)(&mutex_map_lock);
		return &mutex->data[0];
  }
	else {
	  for( i=0; i<mutex->stack_count; i++ ){
	  	
	  	if(comp_stack(call_stack, mutex->stacks[i])) {
	  		found = 1;
	  		break;
	  	}		
	  }
	  // if call site added already , add new one
	  if(!found){
	  	add_call_stack(mutex, call_stack);
			WRAP(pthread_mutex_unlock)(&mutex_map_lock);
	  	return &mutex->data[mutex->stack_count-1];
	  }
	  else {
			WRAP(pthread_mutex_unlock)(&mutex_map_lock);
	  	return &mutex->data[i]; //index of the matching call stack;
	  }
  }
}
	
#if 0

void futex_start_timestamp( my_mutex_t *mutex ) 
{
	int idx = getThreadIndex(); //TODO: fix this, add thread index
	struct timeinfo *st = &mutex->futex_start[idx];
	//start(&(mutex->futex_start[idx]));
	start(st);
}


void add_futex_wait( my_mutex_t *mutex )
{
	int idx = getThreadIndex();//TODO: get it once per wait time
	struct timeinfo end;
	struct timeinfo *st = &mutex->futex_start[idx];
	//mutex->futex_wait[idx] = stop(&(mutex->futex_start[idx]), &end);
	double elapse = stop(st, &end); 
	mutex->futex_wait[idx] += elapsed2ms(elapse);
}

void add_cond_wait( my_mutex_t *mutex )
{
	int idx = getThreadIndex();//TODO: get it once per wait time
	struct timeinfo end;
	struct timeinfo *st = &mutex->futex_start[idx];
	//mutex->futex_wait[idx] = stop(&(mutex->futex_start[idx]), &end);
	double elapse = stop(st, &end); 
	mutex->cond_futex_wait[idx] += elapsed2ms(elapse);
}


void trylock_first_timestamp( my_mutex_t *mutex ) 
{
	int idx= getThreadIndex(); //TODO: get it once per wait time
	struct timeinfo *st = &mutex->trylock_first[idx];
	if(!mutex->trylock_flag[idx]){
		start(st);
		mutex->trylock_flag[idx] = 1;
	}
}

void add_trylock_fail_time( my_mutex_t *mutex )
{
	int idx = getThreadIndex();
	assert(mutex->trylock_flag[idx] == 1);
	struct timeinfo end;
	struct timeinfo *st = &mutex->trylock_first[idx];
	double elapse = stop(st, &end); 
	mutex->trylock_wait_time[idx] += elapsed2ms(elapse);
	mutex->trylock_flag[idx] = 0;
}

void inc_trylock_fail_count( my_mutex_t *mutex ) {
	int idx = getThreadIndex();	
	mutex->trylock_fail_count[idx]++;
}



#else
void futex_start_timestamp( mutex_meta_t *mutex ) 
{
	int idx = getThreadIndex(); //TODO: fix this, add thread index
	struct timeinfo *st = &mutex->futex_start[idx];
	//start(&(mutex->futex_start[idx]));
	start(st);
}

void add_futex_wait( mutex_meta_t *mutex )
{
	int idx = getThreadIndex();//TODO: get it once per wait time
	struct timeinfo end;
	struct timeinfo *st = &mutex->futex_start[idx];
	//mutex->futex_wait[idx] = stop(&(mutex->futex_start[idx]), &end);
	double elapse = stop(st, &end); 
	mutex->futex_wait[idx] += elapsed2ms(elapse);
}

void add_cond_wait( mutex_meta_t *mutex )
{
	int idx = getThreadIndex();//TODO: get it once per wait time
	struct timeinfo end;
	struct timeinfo *st = &mutex->futex_start[idx];
	//mutex->futex_wait[idx] = stop(&(mutex->futex_start[idx]), &end);
	double elapse = stop(st, &end); 
	mutex->cond_futex_wait[idx] += elapsed2ms(elapse);
}

void trylock_first_timestamp( mutex_meta_t *mutex ) 
{
	int idx= getThreadIndex(); //TODO: get it once per wait time
	struct timeinfo *st = &mutex->trylock_first[idx];
	if(!mutex->trylock_flag[idx]){
		start(st);
		mutex->trylock_flag[idx] = 1;
	}
}



void add_trylock_fail_time( mutex_meta_t *mutex )
{
	int idx = getThreadIndex();
	assert(mutex->trylock_flag[idx] == 1);
	struct timeinfo end;
	struct timeinfo *st = &mutex->trylock_first[idx];
	double elapse = stop(st, &end); 
	mutex->trylock_wait_time[idx] += elapsed2ms(elapse);
	mutex->trylock_flag[idx] = 0;
}


void inc_trylock_fail_count( mutex_meta_t *mutex ) {
	int idx = getThreadIndex();	
	mutex->trylock_fail_count[idx]++;
}

#endif

void append( my_mutex_t *mut ) {
  WRAP(pthread_mutex_lock)(&mutex_list_lock);  
  mutex_list.push_back(mut);
	WRAP(pthread_mutex_unlock)(&mutex_list_lock);
}

int back_trace(long stacks[ ], int size)
{
  void * stack_top;/* pointing to current API stack top */
  struct stack_frame * current_frame;
  int    i, found = 0;

  /* get current stack-frame */
  current_frame = (struct stack_frame*)(__builtin_frame_address(0));
  
  stack_top = &found;/* pointing to curent API's stack-top */
  
  /* Omit current stack-frame due to calling current API 'back_trace' itself */
  for (i = 0; i < 1; i++) {
    if (((void*)current_frame < stack_top) || ((void*)current_frame > __libc_stack_end)) break;
    current_frame = current_frame->prev;
  }
  
  /* As we pointing to chains-beginning of real-callers, let's collect all stuff... */
  for (i = 0; i < size; i++) {
    /* Stop in case we hit the back-stack information */
    if (((void*)current_frame < stack_top) || ((void*)current_frame > __libc_stack_end)) break;
    /* omit some weird caller's stack-frame info * if hits. Avoid dead-loop */
    if ((current_frame->caller_address == 0) || (current_frame == current_frame->prev)) break;
    /* make sure the stack_frame is aligned? */
    if (((unsigned long)current_frame) & 0x01) break;

    /* Ok, we can collect the guys right now... */
    stacks[found++] = current_frame->caller_address;
    /* move to previous stack-frame */
    current_frame = current_frame->prev;
  }

  /* omit the stack-frame before main, like API __libc_start_main */
  if (found > 1) found--;

  stacks[found] = 0;/* fill up the ending */

  return found;
}



#ifdef REPORT
void report() {
	std::list<my_mutex_t*>::iterator it;
	WAIT_TIME_TYPE contention = 0; 
	int fail_count = 0;
	WAIT_TIME_TYPE trylock_delay = 0;

	WAIT_TIME_TYPE t_waits[M_MAX_THREADS] = {0};
	std::cout << "Report...\n";

	std::fstream fs;
	fs.open("mutex.csv", std::fstream::out);
  //mutex_id, call stacks, futex_wait, cond_wait, trylock_wait, trylock fail count
	fs << "mutex_id, call stacks, futex_wait, cond_wait, trylock_wait, trylock_fail_count"<< std::endl;
	int id = 0; // mutex_id just for reporting
	for(it = mutex_list.begin(); it != mutex_list.end(); ++it ){
  	my_mutex_t *m = *it;
		id++;
		//std::cout << m->stack_count << std::endl;
		int i;
		for( i=0; i< m->stack_count; i++ ){
			//std::cout<<"stack: " << i << std::endl;
			int j=0;
			fs << id << ",";
			while(m->stacks[i][j] != 0 ) {
				//printf("%#lx\n", m->stacks[i][j]);	
				//std::cout << std::hex << m->stacks[i][j] << std::endl;
				fs<< " " << std::hex << m->stacks[i][j];
				j++;
			}
			fs <<", ";
 			WAIT_TIME_TYPE total = 0;
			WAIT_TIME_TYPE cond_total = 0;
			WAIT_TIME_TYPE try_total = 0;
			int try_fail_count = 0;
			int tid;
			for( tid=0; tid<M_MAX_THREADS; tid++ ){
				total+= m->data[i].futex_wait[tid];
				cond_total+= m->data[i].cond_futex_wait[tid];
				try_total+= m->data[i].trylock_wait_time[tid];
				try_fail_count += m->data[i].trylock_fail_count[tid];
			}

			//std::cout << total << std::endl;
			fs << std::dec <<  total << "," << cond_total << "," << try_total << "," << try_fail_count <<  std::endl;
		}
  }
	fs.close();
}


#endif


