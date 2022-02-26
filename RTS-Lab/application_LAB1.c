#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "stdio.h"
#include "stdlib.h"

#define BUFF_SIZE 200
#define WCET 1

// frequency indexes of brother john tune
int indexes[32] =  { 0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 
                     5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

// stores the periods associated with indexes from -10 to 14 ('not' brother john tune)
int periods_micro[32] = { 2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 
                          1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

/*************************************************
*   Object declarations and intializations       *
*************************************************/
typedef struct {
    Object super;
    int count;
    int sum;
    char buf[100];  // the character arrary read from keyboard
    char c;
    int key;
    int startIndex;
} App;
App app = {initObject(), 0, 0,'X', 0, 0, 0};

typedef struct {
    Object super;
    int key;        /* the key we are playing brother john with (between -5 and 5 */
    int index;      /* where we are in the brother john */
    char volume;    /* represents the volume (value we write to the DAC)... NOTE must be between 1 and 20*/
    int mute;       /*1 representes the the music player is muted*/
    int enableDeadline; /* if '1' then a deadlines are enabled, intially deadlines are disabled */
} MusicPlayer;
MusicPlayer player = {initObject(), 0, 0, 1, 0, 0};

typedef struct {
    Object super;
    int period;                     /* 1300 microsec*/
    int background_loop_range;      /* Number of times to execute the empty loop, default = 1000 */
    int enableDeadline;             /* if '1' then a deadlines are enabled, intially deadlines are disabled */
} Distortion;
Distortion distortion = {initObject(), 1300, 1000, 0}; // initialize a distortion for the 1K sound

typedef struct {
    Object super;
    int sec;
    int enabled; // if '1' then the clock is enabled, default = 1(enabled)
} Clock;
Clock clock = {initObject(), 0, 1};

typedef struct {
    int sec;
} ExecutionTime;
ExecutionTime executionTime = {0};

/*************************************************
*   function Prototypes for interrupt Handlers   *
*************************************************/
void controlVolume(MusicPlayer*, int); //used to increase/decrease the volume and mute
void receiver(MusicPlayer*, int );

/*************************************************
*   function Prototypes for Normal Functions     *
*************************************************/
void generate1KPeriodicSound(MusicPlayer*, int); //used to generate 1k sound
void disturb(Distortion*, int); // disturb method of distortion class iterates a background_loop_range times each period (1300 microsec)
void controlDistortion(Distortion*, int );
void controlDeadline(Distortion*, int );
void startTick( Clock*, int);
void getTime(Clock*, ExecutionTime* ); // a normal 'C' fashioned function
void resetTimer( Clock*, int ); // reset the timer back to '0'
void enableTick( Clock*, int);
void disableTick( Clock*, int);

/**************************************************
*  SCI and CAN interrupt handler initializations  *
**************************************************/
Serial sci1 = initSerial(SCI_PORT0, &player, controlVolume);
Can can1 = initCan(CAN_PORT0, &player, receiver);

/*************************************************
*   Interrupt handler Definitions                *
*************************************************/
//Control the volume of music using i (increase), d(decrease) and m (mute) keys from the keyboard
void controlVolume(MusicPlayer* player, int c){
    if (c == 'i'){
        if(player->volume < 19){
            // TODO need a mutex here
            player->volume++;
        }
    }
    else if(c == 'd'){
        if(player->volume > 2){
            // TODO need a mutex here
            player->volume--;
        }
    }
    else if(c == 'm'){
        player->mute = !player->mute;
    }
    else if( c == 'q' || c == 'a' ){
        ASYNC(&distortion, controlDistortion, c);
    }
    else if( c == 'w' || c == 's' ){
        ASYNC(&distortion, controlDeadline, c);
    }

}

void receiver(MusicPlayer *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can1, &msg);
    SCI_WRITE(&sci1, "Can msg received: ");
    SCI_WRITE(&sci1, msg.buff);
}

/*************************************************
*   NOrmal method's Definitions                  *
*************************************************/
// generate1KPeriodicSound writes a sequences of 1s in a period of 0.5 Milliseconds (i.e 1KHz sound)
void generate1KPeriodicSound(MusicPlayer* player, int alternate){
#if WCET
    SYNC(&clock, resetTimer, 0); // reset the timer to 0
    SYNC(&clock, enableTick, 0); // enable the clock if disabled
    SYNC(&clock, startTick, 0); // start to tick
#endif

    //generate a sequence of 1 and 0 to a memory mapped location
    // Note the char* (we are going to write integer into it), but char doesn't mean
    // a character but a byte
    char* soundCardPointer = (char*)0x4000741C;
    // write the volume to the Memory-mapped region
    *soundCardPointer = (alternate == 0 || player->mute == 1 ? 0: player->volume);
    // tell the next round to write the volume or the zero
    alternate = !alternate;

#if WCET
    // Before iterating calculate the execution time
    SYNC(&clock, disableTick, 0); // if the clock is disabled the periodic tick event returns/stops ticking
    getTime(&clock,&executionTime); // store the execution time

    int execTime[500];
    char output[BUFF_SIZE];
    unsigned int sum = 0;
    static int count; // intialized to 0
    if (count < 500){ // run for 500 times only
        execTime[count] = executionTime.sec; // collect all 500 execution timings in the buffer
        count ++;
        SEND(USEC(500),USEC(100), player, generate1KPeriodicSound, alternate); // deadline=100us with periodicity=500us
    }
    // Once done with 500 samples calulate the Average & Max WCET

    for(int i= 0; i< 500;i++)
        sum = sum + execTime[i];

    snprintf(output, BUFF_SIZE, "Average WCET= %d\n", (unsigned int)sum / 500);
    SCI_WRITE(&sci1, output);

    // TODO: MAX WCET

#else
    //shedule this process to write a 1kHz => period = 1/2k = 500 microseconds
    if (player->enableDeadline)
        SEND(USEC(500),USEC(100), player, generate1KPeriodicSound, alternate); // deadline=100us with periodicity=500us
    else
        AFTER(USEC(500), player, generate1KPeriodicSound, alternate); // schedule this method to run after a specified period (500 microsec)
#endif
}

// generate a distortion, executes 'background_loop_range' number of times an empty loop
void disturb(Distortion* distortion, int unused){

#if WCET
    SYNC(&clock, resetTimer, 0); // reset the timer to 0
    SYNC(&clock, enableTick, 0); // enable the clock if disabled
    SYNC(&clock, startTick, 0); // start to tick
#endif

    for (int i = 0; i < distortion->background_loop_range; i++){}

#if WCET
    // Before iterating calculate the execution time
    SYNC(&clock, disableTick, 0); // if the clock is disabled the periodic tick event returns/stops ticking
    getTime(&clock,&executionTime); // store the execution time

    int execTime[500];
    char output[BUFF_SIZE];
    unsigned int sum = 0;
    static int count; // intialized to 0
    if (count < 500){ // run for 500 times only
        count ++;
        SEND(USEC(distortion->period),USEC(1300), distortion, disturb, unused); // deadline=1300us with periodicity=1300us
    }
    // Once done with 500 samples calulate the Average & Max WCET

    for(int i= 0; i< 500;i++)
        sum = sum + execTime[i];

    snprintf(output,BUFF_SIZE, "Average WCET= %d\n",(unsigned int)sum / 500);
    SCI_WRITE(&sci1, output);

    // TODO: MAX WCET

#else
    if (distortion->enableDeadline)
        SEND(USEC(distortion->period),USEC(1300), distortion, disturb, unused); // deadline=1300us with periodicity=1300us
    else
        AFTER(USEC(distortion->period), distortion, disturb, unused); // schedule this method to run after a specified period (1300 microsec)
#endif
}

//increment and decrement the background range based on the users input
void controlDistortion(Distortion* distortion, int increaseDecrease){
    char output[BUFF_SIZE];
    if(increaseDecrease == 'q') // increase the load
        distortion->background_loop_range += 500;
    else if(increaseDecrease == 'a') // decrease the load
        distortion->background_loop_range += 500;

    snprintf(output,BUFF_SIZE,"background_loop_range = %d\n",distortion->background_loop_range);
    SCI_WRITE(&sci1, output);
}

// Enable or Disable the deadlines for both the tasks simultaneously
void controlDeadline(Distortion* distortion, int enableDisable){

    if(enableDisable == 'w'){ // enabling the deadlines simultaneously
        distortion->enableDeadline = 1;
        player.enableDeadline = 1;
    }
    else if(enableDisable == 's'){ // disabling the deadlines simultaneously
        distortion->enableDeadline = 0;
        player.enableDeadline = 0;
    }
}

// A simple clock functionality
void startTick( Clock *self, int unused) {
    if(self->enabled){
        self->sec++;
        AFTER( SEC(1), self, startTick, 0);
    }
}

// A normal 'C' fashioned function to get time
void getTime( Clock *self, ExecutionTime *executionTime) {
    executionTime->sec = self->sec;
}

// function to reset the time back to 0 seconds
void resetTimer( Clock *self, int unused) {
    self->sec = 0;
}

// function to stop the clock
void disableTick( Clock *self, int unused) {
    self->enabled = 0;
}

// function to start the clock
void enableTick( Clock *self, int unused) {
    self->enabled = 1;
}


/*************************************************
*   TINY-TIMBER invoking method definitions      *
*************************************************/

/*
 * start1KSoundWithDistortion will trigger the tone generating task and a background distortion task
 */
void start1KSoundWithDistortion(MusicPlayer *self, int alternate) {

    CAN_INIT(&can1);
    SCI_INIT(&sci1);
    SCI_WRITE(&sci1, "Playing 1K sound with distortion\n");
    ASYNC(self, generate1KPeriodicSound, alternate);
    ASYNC(&distortion, disturb, 0);  // run a background load tast concurrently
}

void startDistortionOnly(Distortion *self, int alternate) {
    CAN_INIT(&can1);
    SCI_INIT(&sci1);
    SCI_WRITE(&sci1, "Playing 1K sound with distortion\n");

    ASYNC(self, disturb, 0);  // run a background load tast concurrently

}

/*
 * start1KSound will trigger the tone generating task
 */
void start1KSound(MusicPlayer *self, int alternate) {
    CAN_INIT(&can1);
    SCI_INIT(&sci1);
    SCI_WRITE(&sci1, "Playing 1K sound\n");

    ASYNC(self, generate1KPeriodicSound, alternate);
}

void start1KSoundWithDeadline(MusicPlayer *self, int alternate) {
    CAN_INIT(&can1);
    SCI_INIT(&sci1);
    SCI_WRITE(&sci1, "Playing 1K sound with deadlines\n");
    BEFORE(USEC(100),self, generate1KPeriodicSound, alternate); // start a tone ikhz generating task with a deadline 100us
    BEFORE(USEC(1300),&distortion, disturb, 0);  // run a background load tast concurrently with deadline 1300us
}


void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can1);
    SCI_INIT(&sci1);
    SCI_WRITE(&sci1, "Hello, hello...\n");

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can1, &msg);
}

int main() {
    // Lab 1. solution
    INSTALL(&sci1, sci_interrupt, SCI_IRQ0);
    INSTALL(&can1, can_interrupt, CAN_IRQ0);

    //TINYTIMBER(&app, startApp, 0);

    //TINYTIMBER(&player, start1KSound, 1);
    //TINYTIMBER(&player, start1KSoundWithDistortion, 1);
    //TINYTIMBER(&player, start1KSoundWithDeadline, 1); // Note: When Enabling this, intiatialize enableDeadline member to '1' aswell

    /* step 4 */
    TINYTIMBER(&player, start1KSound, 1);
    TINYTIMBER(&distortion, startDistortionOnly, 1);



    return 0;
}
