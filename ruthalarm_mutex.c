/*
 * alarm_mutex.c
 *
 * This is an enhancement to the alarm_thread.c program, which
 * created an "alarm thread" for each alarm command. This new
 * version uses a single alarm thread, which reads the next
 * entry in a list. The main thread places new requests onto the
 * list, in order of absolute expiration time. The list is
 * protected by a mutex, and the alarm thread sleeps for at
 * least 1 second, each iteration, to ensure that the main
 * thread can lock the mutex to add new work to the list.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"

/*
 * The "alarm" structure now contains the time_t (time since the
 * Epoch, in seconds) for each alarm, so that they can be
 * sorted. Storing the requested number of seconds would not be
 * enough, since the "alarm thread" cannot tell how long it has
 * been on the list.
 */
typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    char                type[20];
    int                 Alarm_ID;
    time_t              time;   /* seconds from EPOCH */
    char                message[128];
    char                Alarm_catagory[20];
    char                Message_catagory[20];
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
alarm_t *alarm_list = NULL;



/*
 * The alarm thread's start routine.
 */
void *alarm_thread (void *arg)
{
    alarm_t *alarm;
    int sleep_time;
    time_t now;
    int status;

    /*
     * Loop forever, processing commands. The alarm thread will
     * be disintegrated when the process exits.
     */
    while (1) {
        status = pthread_mutex_lock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Lock mutex");
        alarm = alarm_list;

        /*
         * If the alarm list is empty, wait for one second. This
         * allows the main thread to run, and read another
         * command. If the list is not empty, remove the first
         * item. Compute the number of seconds to wait -- if the
         * result is less than 0 (the time has passed), then set
         * the sleep_time to 0.
         */
        if (alarm == NULL)
            sleep_time = 1;
        else {
            alarm_list = alarm->link;
            now = time (NULL);
            if (alarm->time <= now)
                sleep_time = 0;
            else
                sleep_time = alarm->time - now;
#ifdef DEBUG
            printf ("[waiting: %d(%d)\"%s\"]\n", alarm->time,
                sleep_time, alarm->message);
#endif
            }

        /*
         * Unlock the mutex before waiting, so that the main
         * thread can lock it to insert a new alarm request. If
         * the sleep_time is 0, then call sched_yield, giving
         * the main thread a chance to run if it has been
         * readied by user input, without delaying the message
         * if there's no input.
         */
        status = pthread_mutex_unlock (&alarm_mutex);
        if (status != 0)
            err_abort (status, "Unlock mutex");
        if (sleep_time > 0)
            sleep (sleep_time);
        else
            sched_yield ();

        /*
         * If a timer expired, print the message and free the
         * structure.
         */
        if (alarm != NULL) {
            printf ("(%d) %s\n", alarm->seconds, alarm->message);
            free (alarm);
        }
    }
}

int main (int argc, char *argv[])
{
    int status;
    char line[128];
    alarm_t *alarm, **last, *next, *previous;
    pthread_t thread;
    char start[20] = "Start_Alarm";
    char cancel[20] = "Cancel_Alarm";
    char s[20],ss[20],sss[20];

    status = pthread_create (
        &thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");
    while (1) {
        printf ("alarm> ");
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));
        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        /*
         * Parse input line into seconds (%d) and a message
         * (%128[^\n]), consisting of up to 128 characters
         * separated from the seconds by whitespace.
         */
        int x = sscanf (line, "%s %d %s %128[^\n]", alarm->type, &alarm->seconds,  
            alarm->Message_catagory, alarm->message);
        int y = sscanf(alarm->type, "%[^'('](%[^')'],%s",s,ss,sss);
        alarm->Alarm_ID  = atoi(ss);
        /*char * token = strtok(s, "(");
        
        strcpy(alarm->type,token);
        token = strtok(NULL,")");
        alarm->Alarm_ID = atoi(token);
         */
   
        if ((x != 1 && x != 4 ) ||y<2 || (strcmp(alarm->type,start)==0 || strcmp(alarm->type,cancel) ==0)) {
            fprintf (stderr, "Bad command\n");
            printf ("s: %s ss: %s sss: %s start: %s cancel: %s\n type:%s id:%d\n",s,ss,sss,start,cancel,alarm->type,alarm->Alarm_ID);
            printf("same with start?: %d x: %d y: %d \n",strcmp(alarm->type,start),x,y);
            free (alarm);
        } else if(x==1 && strcmp(alarm->type,cancel) ==0){
                //cancel code here
                last = &alarm_list;
                next = *last;
                previous = *last;
                while (next != NULL) {
                    if (next->Alarm_ID == alarm->Alarm_ID) {
                        previous->link = previous->link->link;
                        //*last = ;
                     break;
                 }
                    last = &next->link;
                    next = next->link;
                    if(next->Alarm_ID != previous->link->Alarm_ID)
                        previous = previous->link;
                }
        }else {
            status = pthread_mutex_lock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Lock mutex");
            alarm->time = time (NULL) + alarm->seconds;

            /*
             * Insert the new alarm into the list of alarms,
             * sorted by expiration time.
             */
            last = &alarm_list;
            next = *last;
            
            while (next != NULL) {
                if (next->Alarm_ID >= alarm->Alarm_ID) {
                    printf("alarm: %d\n",alarm->Alarm_ID);
                    alarm->link = next;
                    *last = alarm;
                    break;
                }
                last = &next->link;
                next = next->link;
            }
            /*
             * If we reached the end of the list, insert the new
             * alarm there. ("next" is NULL, and "last" points
             * to the link field of the last item, or to the
             * list header).
             */
            if (next == NULL) {
                *last = alarm;
                alarm->link = NULL;
            }
#ifdef DEBUG
            printf ("[list: ");
            for (next = alarm_list; next != NULL; next = next->link)
                printf ("%d(%d)[\"%s\"] ", next->time,
                    next->time - time (NULL), next->message);
            printf ("]\n");
#endif
            status = pthread_mutex_unlock (&alarm_mutex);
            if (status != 0)
                err_abort (status, "Unlock mutex");
        }
    }
}
