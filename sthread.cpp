#include <setjmp.h> // setjmp( )
#include <signal.h> // signal( )
#include <unistd.h> // sleep( ), alarm( )
#include <stdio.h>  // perror( )
#include <stdlib.h> // exit( )
#include <iostream> // cout
#include <string.h> // memcpy
#include <queue>    // queue

#define scheduler_init( ) {			\
    if ( setjmp( main_env ) == 0 )		\
      scheduler( );				\
  }

#define scheduler_start( ) {			\
    if ( setjmp( main_env ) == 0 )		\
      longjmp( scheduler_env, 1 );		\
  }

// TODO: to be implemented in this assignment
#define capture( ) { \
   register void *sp asm ("sp"); \
   register void *bp asm ("bp"); \
   cur_tcb->size = (int)((long long int)bp - (long long int)sp); \
   cur_tcb->sp = sp; \
   /* free existing stack if allocated */ \
   if (cur_tcb->stack != NULL) { \
      free(cur_tcb->stack); \
   } \
   /* allocate memory for stack copy */ \
   cur_tcb->stack = malloc(cur_tcb->size); \
   if (!cur_tcb->stack) { \
       perror("malloc failed"); \
       exit(-1); \
   } \
   /* copy stack data into tcb */ \
   memcpy(cur_tcb->stack, sp, cur_tcb->size); \
   /* push tcb to thread queue */ \
   thr_queue.push(cur_tcb); \
  }

// TODO: to be implemented in this assignment   
#define sthread_yield( ) { \
   if (alarmed) { \
     alarmed = false; \
     /* save context skip capture if returning */ \
     if (setjmp(cur_tcb->env) == 0) { \
       capture(); \
       /* jump to scheduler */ \
       longjmp(scheduler_env, 1); \
     } \
     /* restore stack from tcb */ \
     memcpy(cur_tcb->sp, cur_tcb->stack, cur_tcb->size); \
   } \
  }

#define sthread_init( ) {					\
    if ( setjmp( cur_tcb->env ) == 0 ) {			\
      capture( );						\
      longjmp( main_env, 1 );					\
    }								\
    memcpy( cur_tcb->sp, cur_tcb->stack, cur_tcb->size );	\
  }

#define sthread_create( function, arguments ) { \
    if ( setjmp( main_env ) == 0 ) {		\
      func = &function;				\
      args = arguments;				\
      thread_created = true;			\
      cur_tcb = new TCB( );			\
      longjmp( scheduler_env, 1 );		\
    }						\
  }

#define sthread_exit( ) {			\
    if ( cur_tcb->stack != NULL )		\
      free( cur_tcb->stack );			\
    longjmp( scheduler_env, 1 );		\
  }

using namespace std;

static jmp_buf main_env;
static jmp_buf scheduler_env;

// Thread control block
class TCB {
public:
  TCB( ) : sp( NULL ), stack( NULL ), size( 0 ) { }
  jmp_buf env;  // the execution environment captured by set_jmp( )
  void* sp;     // the stack pointer 
  void* stack;  // the temporary space to maintain the latest stack contents
  int size;     // the size of the stack contents
};
static TCB* cur_tcb;   // the TCB of the current thread in execution

// The queue of active threads
static queue<TCB*> thr_queue;

// Alarm caught to switch to the next thread
static bool alarmed = false;
static void sig_alarm( int signo ) {
  alarmed = true;
}

// A function to be executed by a thread
void (*func)( void * );
void *args = NULL;
static bool thread_created = false;

static void scheduler( ) {
  // invoke scheduler
  if ( setjmp( scheduler_env ) == 0 ) {
    cerr << "scheduler: initialized" << endl;
    if ( signal( SIGALRM, sig_alarm ) == SIG_ERR ) {
      perror( "signal function" );
      exit( -1 );
    }
    longjmp( main_env, 1 );
  }

  // check if it was called from sthread_create( )
  if ( thread_created == true ) {
    thread_created = false;
    ( *func )( args );
  }

  // restore the next thread's environment
  if ( ( cur_tcb = thr_queue.front( ) ) != NULL ) {
    thr_queue.pop( );

    // allocate a time quontum of 5 seconds
    alarm( 5 );

    // return to the next thread's execution
    longjmp( cur_tcb->env, 1 );
  }

  // no threads to schedule, simply return
  cerr << "scheduler: no more threads to schedule" << endl;
  longjmp( main_env, 2 );
}