/*
 * New_Alarm_Cond.c
 *
 * New_Alarm_Cond.c is an enhancement to the alarm_cond.c program,
 * which a condition variable to the alarm_mutex.c program. This
 * new version will have two periodic display threads in addition
 * to the main and alarm thread in the alarm_cond.c program.
 */
#include <pthread.h>
#include <time.h>
#include "errors.h"
#include <semaphore.h>
#include <stdbool.h>

typedef struct alarm_tag {
    struct alarm_tag    *link;
    int                 seconds;
    int                 mssg_num; 
    int                 cancel; 
    int                 replacable;
    time_t              time;   /* Seconds from EPOCH */
    char                message[128]; /* Message */
} alarm_t;

pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;
alarm_t *a_list = NULL;
int curnt_alarm = 0;

/*
 * In charge of printing the list of alarms. Since the function
 * needs to access the alarm list, we used a semaphore around
 * the code that prints the alarm list through the use of a
 * simple for loop.
 */
sem_t rw_mutex;
sem_t mutex;
int read_count = 0;

void print_a_list() {
    alarm_t *next;

    sem_wait(&mutex);
    read_count++;
    if(read_count == 1)
        sem_wait(&rw_mutex);
    sem_post(&mutex);

    printf ("[list: ");
    for (next = a_list; next != NULL; next = next->link)
        printf ("%ld(%ld)[\"%s\"]", next->time,
            next->time - time (NULL), next->message);
    printf ("]\n");

    sem_wait(&mutex);
    read_count--;
    if(read_count == 0)
        sem_post(&rw_mutex);
    sem_post(&mutex);
}

/* Fetches the alarm with the given alarm number to it. */
alarm_t *get_alarm_at(int m_id) {
    alarm_t *next;

    if(a_list != NULL) {
        for(next = a_list; next != NULL; next = next->link) {
            if(next->mssg_num == m_id)
                return next;
        }
    }
    return 0;
}

/*
 * Checks whether an alarm exists in the alarm list with the message
 * number of a newly received alarm request and return true or false.
 */
int message_id_exists(int m_id) {
    alarm_t *alarm = get_alarm_at(m_id);
    
    if (alarm == NULL)
        return 0;
    else
        return 1;

}

/*
 * If an alarm request of Type A is received and there exists an
 * alarm of Type A in the alarm list with the same message number,
 * then the old alarm is replacable by this function.
 */
void find_and_replace(alarm_t *new_alarm) {
    alarm_t *old_alarm;
    int status;

    sem_wait(&rw_mutex);

    old_alarm = get_alarm_at(new_alarm->mssg_num);
    old_alarm->seconds = new_alarm->seconds;
    old_alarm->time = time(NULL) + new_alarm->seconds;
    old_alarm->replacable = 1;
    strcpy(old_alarm->message , new_alarm->message);

    sem_post(&rw_mutex);
}

/*
 * Used to remove any nodes (alarm requests) from the alarm list.
 */
void cancel_alarm (alarm_t *alarm) {
    alarm_t *prev = a_list;

    sem_wait(&rw_mutex);

    if(a_list != NULL) {
        if(a_list != alarm) {
            while(prev->link != NULL && prev->link != alarm)
                prev = prev->link;

            if(prev->link != NULL)
                prev->link = prev->link->link;
        } 
        else {
            if(a_list->link != NULL)
                a_list = a_list->link;
            else
                a_list = NULL;
        }
    }

    sem_post(&rw_mutex);
}

/*
 * Wakes the alarm thread if it is not busy, or if a new alarm
 * arrives before the one on which the alarm thread is waiting for.
 * CHANGED SOME WHILE LOOPS 
 */
void alarm_insert(alarm_t *alarm) {
    int s;
    alarm_t **last;
    alarm_t *next;
    last = &a_list;
    next = *last;
    bool flag = true;
    sem_wait(&rw_mutex);
    
    
    while (next) {
        if (next->mssg_num >= alarm->mssg_num) {
            if (flag == true){
                alarm->link = next;
                *last = alarm;
                break;
            }
        }
        last = &next->link;
        next = next->link;
    }
    
    if (next == NULL) {
        *last = alarm;
        alarm->link = NULL;
    }

    
    printf("First Alarm Request With Message Number (%d) Received at <%ld>: <%d %s>\n",
        alarm->mssg_num, time(NULL), alarm->seconds, alarm->message);

    
    if (curnt_alarm == 0) {
        int current = alarm->time;
        curnt_alarm = current;
        s = (pthread_cond_signal (&alarm_cond))- 1;
        s = s+1; //test
        if (s > 0 || s < 0)
            err_abort (s, "Signal cond");

    }

    else if( alarm->time < curnt_alarm){
        int current = alarm->time;
        curnt_alarm = current;
        s = (pthread_cond_signal (&alarm_cond))- 1;
        s = s+1; //test
        if (s > 0 || s < 0)
            err_abort (s, "Signal cond");
    }
    

    sem_post(&rw_mutex);
}

/*
 * Responsible for, as the name suggests, periodically going
 * through the alarm list looking for Type A alarms and printing
 * the appropriate message every Time seconds, where Time is the
 * time of the alarm request originally provided when the alarm
 * request was received.
 */
void *periodic_display_thread(void *alarm_in) {
    int alarm_replacable = 0;
    alarm_t *alarm = (alarm_t*) alarm_in;
    alarm_t *next;
    int status;

    while(1) {
        if(a_list) {
            next = a_list;

            sem_wait(&mutex);
            read_count++;
            if(read_count == 1)
                sem_wait(&rw_mutex);
            sem_post(&mutex);

            while(next->mssg_num != alarm->mssg_num)
                next = next->link;

            if(next == NULL || alarm->cancel > 0) {
                printf("Display thread exiting at <%ld>: <%d %s>\n",
                    time(NULL), alarm->seconds, alarm->message);
                break;
            } else if(next->replacable == 1) {
                if(alarm_replacable == 0) {
                     printf("Alarm With Message Number (%d) replacable at <%ld>: <%d %s>\n",
                    alarm->mssg_num, time(NULL), alarm->seconds, alarm->message);
                }

                printf("Replacement Alarm With Message Number (%d) Displayed at <%ld>: <%d %s>\n",
                    next->mssg_num, time(NULL), next->seconds, next->message);
                alarm_replacable = 1;
                sleep(next->seconds);
            } else {
                printf("Alarm With Message Number (%d) Displayed at <%ld>: <%d %s>\n",
                    alarm->mssg_num, time(NULL), alarm->seconds, alarm->message);
                sleep(alarm->seconds);
            }

            sem_wait(&mutex);
            read_count--;
            if(read_count == 0)
                sem_post(&rw_mutex);
            sem_post(&mutex);

        }
    }
    return 0;
}

/*
 * Tasked with actually processing each alarm request. As such,
 * it will go through the alarm list and use the get_alarm_at
 * function to fetch each alarm, and if it encounters an alarm
 * of Type A, it immediately creates a periodic display thread.
 * If it is an alarm of Type B, it uses the function cancel_alarm
 * to remove the appropriate nodes (alarm requests) from the alarm
 * list. Lastly, it will print a message to the user to let them know
 * the alarm has been processed and the time at which it was processed.
 *  CHANGED ********************
 */
void *alarm_thread(void *arg) {
    pthread_t display_t;
    alarm_t *alarm;
    int status, expired;
    int alarm_stat = 0;
   bool flag = true;
if(flag){
    while(flag) {
        if (a_list == NULL) {
            break;
        }
        else{
            alarm = get_alarm_at(curnt_alarm);

            if(alarm->cancel == 0){
                status = pthread_create(&display_t, NULL, periodic_display_thread, (void *)alarm);
                if(status > 0 || status < 0)
                    err_abort(status, "Create periodic display thread");
            }
            else if (alarm->cancel == 0){
                alarm_stat ++;
            } 
            else {
                cancel_alarm(alarm);
            }
            printf("The Alarm with the message number (%d) was processed at <%ld>: <%d %s>\n",
                alarm->mssg_num, time(NULL), alarm->seconds, alarm->message);
            int stat = pthread_cond_wait (&alarm_cond, &alarm_mutex);
            status = stat;
            if(status > 0 || status < 0)
                err_abort (status, "Cond should be waited on");
        }
    }

}
}

/*
 * In charge of receiving each alarm request and taking appropriate
 * actions with regard to how they should be handled.
 */
int main (int argc, char *argv[]) {
    int status;
    int cancel_message_id = 0;
    char line[256];
    alarm_t *alarm;
    pthread_t thread;

    //semaphore init
    // sem_init(&mutex, 0, 1);
    // sem_init(&rw_mutex, 0, 1);

    status = pthread_create (&thread, NULL, alarm_thread, NULL);
    if (status != 0)
        err_abort (status, "Create alarm thread");

        // Clear the terminal window.
        printf("\e[1;1H\e[2J");

        // Input instructions for the user.
        printf("Please enter an alarm request in the following format: # Message(*) ActualMessage\n");
        printf("# - the number of seconds until the alarm iterates\n");
        printf("* - the message number\n");
        printf("ActualMessage - the message that will be displayed when the alarm iterates\n");
        printf("Example: 2 Message(2) Hello!\n");
        printf("You may add successive alarm requests in the same format at any time during execution\n");
        printf("To cancel an alarm request, use the following format: Cancel: Message(*)\n");
        printf("Disclaimer: Some alternate inputs will be dealt with accordingly,\n\n");

    while (1) {
        if (fgets (line, sizeof (line), stdin) == NULL) exit (0);
        if (strlen (line) <= 1) continue;
        alarm = (alarm_t*)malloc (sizeof (alarm_t));

        if (alarm == NULL)
            errno_abort ("Allocate alarm");

        int insert_command_parse = sscanf(line, "%d Message(%d) %128[^\n]",
            &alarm->seconds, &alarm->mssg_num, alarm->message);
        int cancel_command_parse = sscanf(line, "Cancel: Message(%d)", &cancel_message_id);

        if(insert_command_parse == 3 && alarm->seconds > 0 && alarm->mssg_num > 0) {
            // Check if the mssg_num exits in the alarm list
            if(message_id_exists(alarm->mssg_num) == 0) {
                alarm->time = time (NULL) + alarm->seconds;
                alarm->cancel = 0;
                alarm->replacable = 0;
                curnt_alarm = alarm->mssg_num;

                status = pthread_mutex_lock (&alarm_mutex);
                if (status != 0)
                    err_abort (status, "Lock mutex");

                /*
                 * Insert the new alarm into the list of alarms,
                 * sorted by mssg_num.
                 */
                alarm_insert (alarm);
                pthread_cond_signal(&alarm_cond);

                status = pthread_mutex_unlock (&alarm_mutex);
                if (status != 0)
                    err_abort (status, "Unlock mutex");
            } else {
                find_and_replace(alarm);
                // A3.2.2 Print Statement
                printf("Replacement Alarm Request With Message Number (%d) Received at <%ld>: <%d %s>\n",
                    alarm->mssg_num, time(NULL), alarm->seconds, alarm->message);
            }

        } else if(cancel_command_parse == 1)  {
            if(message_id_exists(cancel_message_id) == 0) {
                printf("Error: No Alarm Request With Message Number (%d) to Cancel!\n", cancel_message_id);
            } else{
                alarm_t *at_alarm = get_alarm_at(cancel_message_id);
                if (at_alarm->cancel > 0)
                    printf("Error: More Than One Request to Cancel Alarm Request With Message Number (%d)!\n", cancel_message_id);
                else {
                    at_alarm->cancel = at_alarm->cancel + 1;
                    curnt_alarm = at_alarm->mssg_num;
                    pthread_cond_signal(&alarm_cond);
                    printf("Cancel Alarm Request With Message Number (%d) Received at <%ld>: <%d %s>\n",
                        at_alarm->mssg_num, time(NULL), at_alarm->seconds, at_alarm->message);
                }
            }
        } else {
            fprintf (stderr, "Invalid command.\n");
            free (alarm);
        }
    }
}
