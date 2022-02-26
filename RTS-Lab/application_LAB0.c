#include "TinyTimber.h"
#include "sciTinyTimber.h"
#include "canTinyTimber.h"
#include "stdio.h"
#include "stdlib.h"

// frequency indexes of brother john tune
int indexes[32] =  { 0, 2, 4, 0, 0, 2, 4, 0, 4, 5, 7, 4, 5, 7, 7, 9, 7, 
                     5, 4, 0, 7, 9, 7, 5, 4, 0, 0, -5, 0, 0, -5, 0};

// stores the periods associated with indexes from -10 to 14 ('not' brother john tune)
int periods_micro[32] = { 2024, 1911, 1803, 1702, 1607, 1516, 1431, 1351, 1275, 1203, 
                          1136, 1072, 1012, 955, 901, 851, 803, 758, 715, 675, 637, 601, 568, 536, 506};

/*************************************************
   Object declarations and intializations
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
App app = { initObject(), 0, 0, 'X', 0, 0 };

/*************************************************
   function Prototypes for interrupt Handlers
*************************************************/
void reader(App*, int);
void reader2a(App*, int );
void reader2b(App*, int );
void printPeriodsKeyKHanler(App*, int);
void receiver(App*, int);

/*************************************************
   function Prototypes for Normal Functions
*************************************************/
void printPeriodsKey0(App*, int);
void printPeriodsKeyK(App*, int);

/*************************************************
   SCI and CAN interrupt handler initializations
*************************************************/
//Serial sci0 = initSerial(SCI_PORT0, &app, reader);
//Serial sci0 = initSerial(SCI_PORT0, &app, reader2a);
//Serial sci0 = initSerial(SCI_PORT0, &app, reader2b);
Serial sci0 = initSerial(SCI_PORT0, &app, printPeriodsKeyKHanler);
Can can0 = initCan(CAN_PORT0, &app, receiver);

/*************************************************
   Interrupt handler Definitions
*************************************************/
void reader(App *self, int c) {
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");
}

void reader2a(App *self, int c) {
    //store the output of the program
    char output[500];

    // the integer read from keyboard; stores the integer version of buf
    int myNum;

    //print the character received to the screen
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");

    // if the user indicated that this is the last digit, add the number to sum
    // and print the running sum along with
    if (c == 'e'){
        // terminate the string
        self->buf[self->count] = '\0';
        // convert the string stored in buf to integer
        myNum = atoi(self->buf);
        // add the number to the sum
        myNum += 13;

        //show the number received from keyboard
        sprintf(output, "The result is is %d\n", myNum);
        SCI_WRITE(&sci0, output);

        // reset our variables to accept another number
        self->count = 0;
        self->buf[self->count] = '\0';
    }
    else{
        //store the character
        self->buf[self->count] = c;
        //increment the character counter
        self->count += 1;
    }
}

void reader2b(App *self, int c){
    //store the output of the program
    char output[500];

    // the integer read from keyboard; stores the integer version of buf
    int myNum;

    //print the character received to the screen
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, c);
    SCI_WRITE(&sci0, "\'\n");

    if(c == 'F'){
        // reset the running sum
        self->sum = 0;
        // show the running sum
        sprintf(output, "The running sum is %d\n", self->sum);
        SCI_WRITE(&sci0, output);
    }
    // if the user indicated that this is the last digit, add the number to sum
    // and print the running sum along with
    else if (c == 'e'){
        // terminate the string
        self->buf[self->count] = '\0';
        // convert the string stored in buf to integer
        myNum = atoi(self->buf);
        // add the number to the sum
        self->sum += myNum;

        //show the number received from keyboard
        sprintf(output, "The entered number is %d\n", myNum);
        SCI_WRITE(&sci0, output);

        // show the running total
        sprintf(output, "The running sum is %d\n", self->sum);
        SCI_WRITE(&sci0, output);

        // reset our variables to accept another number
        self->count = 0;
        self->buf[self->count] = '\0';
    }
    else{
        //store the character
        self->buf[self->count] = c;
        //increment the character counter
        self->count += 1;
    }

}

void printPeriodsKeyKHanler(App* self, int k){
    //store the output of the program
    char output[500];

    // the integer read from keyboard; stores the integer version of buf
    int myNum;

    //print the character received to the screen
    SCI_WRITE(&sci0, "Rcv: \'");
    SCI_WRITECHAR(&sci0, k);
    SCI_WRITE(&sci0, "\'\n");

    // if the user indicated that this is the last digit, add the number to sum
    // and print the running sum along with
    if (k == 'e'){
        // terminate the string
        self->buf[self->count] = '\0';
        // convert the string stored in buf to integer
        myNum = atoi(self->buf);
        // add the number to the sum
        myNum += 13;

        //show the number received from keyboard
        self->key = atoi(self->buf);
        ASYNC(self, printPeriodsKeyK, 0);

        // reset our variables to accept another number
        self->count = 0;
        self->buf[self->count] = '\0';
    }
    else{
        //store the character
        self->buf[self->count] = k;
        //increment the character counter
        self->count += 1;
    }
}

void receiver(App *self, int unused) {
    CANMsg msg;
    CAN_RECEIVE(&can0, &msg);
    SCI_WRITE(&sci0, "Can msg received: ");
    SCI_WRITE(&sci0, msg.buff);
}

/*************************************************
   NOrmal method's Definitions
*************************************************/
// print the periods for key = 0
void printPeriodsKey0(App* app, int count){
    // if we hinitSerialave not reached the last elemnt in the brother john tune
    // display it a){nd reschedule the process.
    if (count < 32 ){
        char output[100];
        sprintf(output, "%d,", periods_micro[indexes[count] + 10]);
        SCI_WRITE(&sci0, output);
        count++;
        // schedule ourown function and pass the next element
        AFTER(SEC(0.5), app, printPeriodsKey0, count++);
    }
}

// print the periods for any key 'k'
void printPeriodsKeyK(App* app, int count){
    // extract the key and ths count (starting value
    int k = app->key;

    // display all the periods for a key
    if (count < 32){
        char output[100];
        sprintf(output, "%d,", periods_micro[indexes[count] + k + 10 ]);
        SCI_WRITE(&sci0, output);
        // schedule ourown function and pass the next element
        count++;
        AFTER(SEC(.05), app, printPeriodsKeyK, count);
    }
}

/*************************************************
   TINY-TIMBER invoking method definitions
*************************************************/

void startPeriodicPrint(App *self, int unused) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Calling periodic print For key 0\n");

    ASYNC(self, printPeriodsKey0, 0);

}

void startApp(App *self, int arg) {
    CANMsg msg;

    CAN_INIT(&can0);
    SCI_INIT(&sci0);
    SCI_WRITE(&sci0, "Hello, hello...\n");

    msg.msgId = 1;
    msg.nodeId = 1;
    msg.length = 6;
    msg.buff[0] = 'H';
    msg.buff[1] = 'e';
    msg.buff[2] = 'l';
    msg.buff[3] = 'l';
    msg.buff[4] = 'o';
    msg.buff[5] = 0;
    CAN_SEND(&can0, &msg);
}

int main() {
    // Lab 0. solution
    // INSTALL(&sci0, sci_interrupt, SCI_IRQ0);
    // INSTALL(&can0, can_interrupt, CAN_IRQ0);

    //TINYTIMBER(&app, startApp, 0);
    TINYTIMBER(&app, startPeriodicPrint, 0); // display the periods of key K= 0 and use interrupt for any key 'k'
    return 0;
}
