output: cc $(New_Alarm_Mutex).o -D_POSIX_PTHREAD_SEMANTICS -lpthread -o a.out

$(New_Alarm_Mutex).o: $(New_Alarm_Mutex).c errors.o
	cc -c $(New_Alarm_Mutex).c

errors.o: cc -c errors.h
	
clean: rm *.o a.out
