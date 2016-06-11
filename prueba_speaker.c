/*************************************************
 * Prueba de sonido con /dev/speaker
 * 
 * 
 * compilar:
 * gcc prueba_speaker.c -o pruebaSonido -lwiringPi
 * 
 * 
 * *********************************/
 
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <wiringPi.h>

#define DEVICE "/dev/speaker"
#define LOW_LEVEL '0'
#define HIGH_LEVEL '1'

int fd_speaker;


/// frecuencias:

const int c = 261;
const int d = 294;
const int e = 329;
const int f = 349;
const int g = 391;
const int gS = 415;
const int a = 440;
const int aS = 455;
const int b = 466;
const int cH = 523;
const int cSH = 554;
const int dH = 587;
const int dSH = 622;
const int eH = 659;
const int fH = 698;
const int fSH = 740;
const int gH = 784;
const int gSH = 830;
const int aH = 880;

void escribe_altavoz(int state)
{
	char c;
	
	if(state) c=HIGH_LEVEL; else c = LOW_LEVEL;
	
	write(fd_speaker,&c,1);
}

void toneManual(int frequency, int duration)
{
    unsigned long period = 1000000/frequency;
    unsigned long length;
    int state = 0;
    for (length = 0; length < (long) duration * 1000; length += period) {
        state = !state;
        escribe_altavoz(state);
        /* The 50uS correspond to the time the rest of the loop body takes.
         * It seems about right, but has not been tuned precisely for
         * a 16MHz ATMega. */
        delayMicroseconds(period - 50);
    }
}

void beep(int note, int duration)
{
    //Play tone on buzzerPin
    toneManual(note, duration);
    delay(20);
}

void firstSection()
{
    beep(a, 500);
    beep(a, 500);
    beep(a, 500);
    beep(f, 350);
    beep(cH, 150);
    beep(a, 500);
    beep(f, 350);
    beep(cH, 150);
    beep(a, 650);

    delay(500);

    beep(eH, 500);
    beep(eH, 500);
    beep(eH, 500);
    beep(fH, 350);
    beep(cH, 150);
    beep(gS, 500);
    beep(f, 350);
    beep(cH, 150);
    beep(a, 650);

    delay(500);
}


int main (int argc, char** argv)
{

    printf ("\n") ;
    printf ("Raspberry Pi - prueba de música en /dev/speaker  \n") ;
    printf ("==================================================\n") ;
    printf ("\n") ;

    if(argc>1)
    {
        struct sched_param sp;
        sp.sched_priority=85;
        pthread_setschedparam(pthread_self(),SCHED_FIFO, &sp);
    }
    
    printf("Abriendo device %s\n",DEVICE);
    
    fd_speaker=open(DEVICE, O_WRONLY);
    
    if(fd_speaker<0) 
    {
		printf("ERROR al abrir el device...\n");
		exit(-1);
	}
    

    //Play first section
    firstSection();

    exit(0);



}
