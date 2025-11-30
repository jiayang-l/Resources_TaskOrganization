#include "tpl_os.h"
#include "Arduino.h"

#define K 5
volatile int queue[K];
volatile int q_head = 0;
volatile int q_tail = 0;
volatile int q_count = 0;

volatile int error = 0; // written by S, read by V
volatile int alarm = 0; // written by B, read by V

void setup()
{
    pinMode(13, OUTPUT);  // initialize digital pin 13 as an output.
    Serial.begin(115200); // 115200 bauds, 8N1 from README.md
    pinMode(A0, INPUT);   // pin A0 as input to get voltage
    Serial.println("Start");
    StartOS(stdAppmode);
}
DeclareAlarm(a100msec);
DeclareAlarm(a500msec);
DeclareAlarm(a125msec);
DeclareResource(Res);

// The TASKs are activated by the alarms "a500msec","a100msec","a125msec

/*  Periodic Sampler C: reads S and A0 each 100 ms */

TASK(TaskS)
{
    GetResource(Res);
    Serial.print("TaskS running:");
    int X = analogRead(A0); // read voltage on pin A0
    if (X < 10 || X > 1013) // Check range and set error
    {
        error = 1;
    }
    else
    {
        error = 0;
    }

    Serial.print("X=");
    Serial.print(X);
    Serial.print(" error=");
    Serial.println(error);
    // Push into queue if there is space
    if (q_count < K)
    {
        queue[q_tail] = X;
        q_tail = (q_tail + 1) % K;
        q_count++;
    }
    else
    {
        // Overflow: report to serial console
        Serial.println("Queue overflow in TaskS");
        // Optionally discard the new sample
    }
    ReleaseResource(Res);
    TerminateTask();
}

TASK(TaskB)
{
    GetResource(Res);
    Serial.print("TaskB running:");
    if (q_count == 0)
    {
        // No data in this window -> define no alarm variation
        alarm = 0;
    }
    else
    {
        int N = queue[q_head];
        int M = queue[q_head];

        while (q_count > 0)
        {
            int data = queue[q_head];
            if (data < N)
                N = data;
            if (data > M)
                M = data;
            q_head = (q_head + 1) % K;
            q_count--;
        }
        Serial.print("M=");
        Serial.print(M);
        Serial.print(" N=");
        Serial.print(N);
        Serial.print("  M-N = ");
        Serial.println(M - N);
        // Now queue is empty, we have N and M for this period
        if ((M - N) > 500)
        {
            alarm = 1;
        }
        else
        {
            alarm = 0;
        }
    }
    Serial.print("alarm=");
    Serial.println(alarm);
    ReleaseResource(Res);
    TerminateTask();
}

/* Task V: LED control every 125 ms */
static int sub = 0;      // sub-counter for slow blinking (1 Hz)
static int LED_mode = 0; // 0=OFF, 1=1Hz SLOWLY, 2=4Hz FAST

TASK(TaskV)
{
    Serial.print("TaskV running:");
    int local_error;
    int local_alarm;
    /* Read shared flags under protection */
    GetResource(Res);
    local_error = error;
    local_alarm = alarm;
    ReleaseResource(Res);

    /* Decide LED mode */
    if (local_error == 1)
    {
        LED_mode = 2; // fast: 4 Hz
    }
    else if (local_alarm == 1)
    {
        LED_mode = 1; // slow: 1 Hz
    }
    else
    {
        LED_mode = 0; // OFF
    }

    /* LED behavior */
    switch (LED_mode)
    {
    case 0: // OFF
        digitalWrite(13, LOW);
        sub = 0; // reset counter
        break;

    case 1: // 1 Hz: toggle every 500 ms → every 4 ticks (125 ms task period)
        if (sub % 4 == 0)
            digitalWrite(13, !digitalRead(13));
        sub = (sub + 1) % 4;
        break;

    case 2: // 4 Hz: toggle every 125 ms
        digitalWrite(13, !digitalRead(13));
        sub = 0; // keep it clean
        break;
    }
    Serial.print("LED_mode=");
    Serial.println(LED_mode);
    TerminateTask();
}
