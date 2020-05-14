//  square.c: Uses timer 2 interrupt to generate a square wave in pin
//  P2.0 and a 75% duty cycle wave in pin P2.1
//  Copyright (c) 2010-2018 Jesus Calvino-Fraga
//  ~C51~

#include <stdio.h>
#include <stdlib.h>
#include <EFM8LB1.h>
#include <string.h>

// ~C51~

#define SYSCLK 72000000L
#define BAUDRATE 115200L

#define OUT0 P2_0   // ON FOR CCW
#define OUT1 P1_7   // ON FOR CW

volatile unsigned char pwm_count=0;
volatile int high0, high1;

char _c51_external_startup (void)

{
    // Disable Watchdog with key sequence
    SFRPAGE = 0x00;
    WDTCN = 0xDE; //First key
    WDTCN = 0xAD; //Second key

    VDM0CN=0x80;       // enable VDD monitor
    RSTSRC=0x02|0x04;  // Enable reset on missing clock detector and VDD

#if (SYSCLK == 48000000L)
    SFRPAGE = 0x10;
		PFE0CN  = 0x10; // SYSCLK < 50 MHz.
		SFRPAGE = 0x00;
#elif (SYSCLK == 72000000L)
    SFRPAGE = 0x10;
    PFE0CN  = 0x20; // SYSCLK < 75 MHz.
    SFRPAGE = 0x00;
#endif

#if (SYSCLK == 12250000L)
    CLKSEL = 0x10;
		CLKSEL = 0x10;
		while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 24500000L)
    CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 48000000L)
    // Before setting clock to 48 MHz, must transition to 24.5 MHz first
		CLKSEL = 0x00;
		CLKSEL = 0x00;
		while ((CLKSEL & 0x80) == 0);
		CLKSEL = 0x07;
		CLKSEL = 0x07;
		while ((CLKSEL & 0x80) == 0);
#elif (SYSCLK == 72000000L)
    // Before setting clock to 72 MHz, must transition to 24.5 MHz first
    CLKSEL = 0x00;
    CLKSEL = 0x00;
    while ((CLKSEL & 0x80) == 0);
    CLKSEL = 0x03;
    CLKSEL = 0x03;
    while ((CLKSEL & 0x80) == 0);
#else
#error SYSCLK must be either 12250000L, 24500000L, 48000000L, or 72000000L
#endif

    P0MDOUT |= 0x10; // Enable UART0 TX as push-pull output
    XBR0     = 0x01; // Enable UART0 on P0.4(TX) and P0.5(RX)
    XBR1     = 0X00;
    XBR2     = 0x40; // Enable crossbar and weak pull-ups

    // Configure Uart 0
#if (((SYSCLK/BAUDRATE)/(2L*12L))>0xFFL)
#error Timer 0 reload value is incorrect because (SYSCLK/BAUDRATE)/(2L*12L) > 0xFF
#endif
    SCON0 = 0x10;
    TH1 = 0x100-((SYSCLK/BAUDRATE)/(2L*12L));
    TL1 = TH1;      // Init Timer1
    TMOD &= ~0xf0;  // TMOD: timer 1 in 8-bit auto-reload
    TMOD |=  0x20;
    TR1 = 1; // START Timer1
    TI = 1;  // Indicate TX0 ready

    // Initialize timer 2 for periodic interrupts
    TMR2CN0=0x00;   // Stop Timer2; Clear TF2;
    CKCON0|=0b_0001_0000; // Timer 2 uses the system clock
    TMR2RL=(0x10000L-(SYSCLK/10000L)); // Initialize reload value
    TMR2=0xffff;   // Set to reload immediately
    ET2=1;         // Enable Timer2 interrupts
    TR2=1;         // Start Timer2 (TMR2CN is bit addressable)

    EA=1; // Enable interrupts
    return 0;
}

// Uses Timer3 to delay <us> micro-seconds.
void Timer3us(unsigned char us)
{
    unsigned char i;               // usec counter

    // The input for Timer 3 is selected as SYSCLK by setting T3ML (bit 6) of CKCON0:
    CKCON0|=0b_0100_0000;

    TMR3RL = (-(SYSCLK)/1000000L); // Set Timer3 to overflow in 1us.
    TMR3 = TMR3RL;                 // Initialize Timer3 for first overflow

    TMR3CN0 = 0x04;                 // Sart Timer3 and clear overflow flag
    for (i = 0; i < us; i++)       // Count <us> overflows
    {
        while (!(TMR3CN0 & 0x80));  // Wait for overflow
        TMR3CN0 &= ~(0x80);         // Clear overflow indicator
    }
    TMR3CN0 = 0 ;                   // Stop Timer3 and clear overflow flag
}

void waitms (unsigned int ms)
{
    unsigned int j;
    for(j=ms; j!=0; j--)
    {
        Timer3us(249);
        Timer3us(249);
        Timer3us(249);
        Timer3us(250);
    }
}

void TIMER0_Init(void)
{
    TMOD&=0b_1111_0000; // Set the bits of Timer/Counter 0 to zero
    TMOD|=0b_0000_0001; // Timer/Counter 0 used as a 16-bit timer
    TR0=0; // Stop Timer/Counter 0
}

void LCD_pulse (void)
{
    P2_5=1;
    Timer3us(40);
    P2_5=0;
}

void LCD_byte (unsigned char x)
{
    // The accumulator in the C8051Fxxx is bit addressable!
    ACC=x; //Send high nible
    P2_1=ACC_7;
    P2_2=ACC_6;
    P2_3=ACC_5;
    P2_4=ACC_4;
    LCD_pulse();
    Timer3us(40);
    ACC=x; //Send low nible
    P2_1=ACC_3;
    P2_2=ACC_2;
    P2_3=ACC_1;
    P2_4=ACC_0;
    LCD_pulse();
}

void WriteData (unsigned char x)
{
    P2_6=1;
    LCD_byte(x);
    waitms(2);
}

void WriteCommand (unsigned char x)
{
    P2_6=0;
    LCD_byte(x);
    waitms(5);
}

void LCD_4BIT (void)
{
    P2_5=0; // Resting state of LCD's enable is zero
    // LCD_RW=0; // We are only writing to the LCD in this program
    waitms(20);
    // First make sure the LCD is in 8-bit mode and then change to 4-bit mode
    WriteCommand(0x33);
    WriteCommand(0x33);
    WriteCommand(0x32); // Change to 4-bit mode

    // Configure the LCD
    WriteCommand(0x28);
    WriteCommand(0x0c);
    WriteCommand(0x01); // Clear screen command (takes some time)
    waitms(20); // Wait for clear screen command to finish.
}

void LCDprint(char * string, unsigned char line, bit clear)
{
    int j;
    WriteCommand(line==2?0xc0:0x80);
    waitms(5);
    for(j=0; string[j]!=0; j++)	WriteData(string[j]);// Write the message
    if(clear) for(; j<16; j++) WriteData(' '); // Clear the rest of the line
}

int getsn (char * buff, int len)
{
    int j;
    char c;

    for(j=0; j<(len-1); j++)
    {
        c=getchar();
        if ( (c=='\n') || (c=='\r') )
        {
            buff[j]=0;
            return j;
        }
        else if ( (c >= '0' && c <= '9') || c=='-' ){
            buff[j]=c;
        }
        else if ( c=='m' ){
            buff[j]=c;
        }
        else
        {
            j--;
        }
    }
    buff[j]=0;
    return len;
}

void Timer2_ISR (void) interrupt 5 {
TF2H = 0; // Clear Timer2 interrupt flag

pwm_count++;
if(pwm_count>100) pwm_count=0;

OUT0=pwm_count>high0?0:1;
OUT1=pwm_count>high1?0:1;
}

float getRPM(int highVal){
    float retVal = 0.00;
    if (highVal >= 15){
        retVal = (float)highVal*0.5238437778;
    } else if (highVal >= 8) {
        retVal = (float)highVal*0.5238437778*0.5;
    } else {
        retVal = (float)highVal*0.0001;
    }

    return retVal;
}

void main (void)
{
    int directionInt = 0;
    float rpm = 0;

    char line1[17] = "Lab 6";
    char line2[17] = "Motor Control";
    char rpmHolder[5] = "";
    char spdHolder[4] = "";

    char buff1[5];
    char buff2[5];
    char direction[4];

    OUT0 = 0;
    OUT1 = 0;
    high0 = 0;
    high1 = 0;
    P3_0 = 1;       // LED is inverted (lights up for ccw)
    P3_1 = 1;       // LED is inverted (lights up for clock-wise)

    printf("\x1b[2J"); // Clear screen using ANSI escape sequence.
    printf("Square wave generator for the EFM8LB1.\r\n"
           "Check pins P2.0 and P2.1 with the oscilloscope.\r\n");

    TIMER0_Init();
    LCD_4BIT();
    LCDprint(line1, 1, 1);
    LCDprint(line2, 2, 1);

    while(1){
        printf("\nInput:\n");
        printf("Positive for clockwise.\n");
        printf("Negative for anti-clockwise.\n");
        printf("Zero for off.\n");
        printf("\"m\" for manual control.\n");
        printf("\n");
        getsn(direction, sizeof(direction));
        printf("\n");
        directionInt = atoi(direction);

        if (strcmp("m", direction)==0){
            printf("\nManual control activated.");
            while (P3_7){
                if (P1_6 == 0){
                    if (high0 > 7){
                        high1 = high0;
                        high0 = 0;
                    } else if (high1 > 7){
                        high0 = high1;
                        high1 = 0;
                    } else {
                        printf("Cannot change while static.");
                    }
                }
                if (P3_2 == 0){ // increase
                    if (high0 > 7){
                        high0 = high0 + 10;
                        high1 = 0;
                    } else if (high1 > 7){
                        high1 = high1 - 10;
                        high0 = 0;
                    } else {
                        high0 = 10;
                        high1 = 0;
                    }
                }

                if (P3_3 == 0){ // decrease
                    if (high0 > 7){
                        high0 = high0 - 10;

                        high1 = 0;
                    } else if (high1 > 7){
                        high1 = high1 + 10;
                        high0 = 0;
                    } else {
                        high0 = 0;
                        high1 = 10;
                    }
                }

                if (high0 <= 7){
                    P3_0 = 1;
                    high0 = 0;
                }
                if (high1 <= 7){
                    P3_1 = 1;
                    high1 = 0;
                }

                if (high0 > 7){
                    P3_1 = 1;       // we are moving ccw
                    P3_0 = 0;       // LED is inverted

                    strcpy(line1, "Anti-Clockwise");
                    LCDprint(line1, 1, 1);

                    if (high0 > 100){
                        high0 = 100;
                    }
                    rpm = getRPM(high0);

                    sprintf(rpmHolder, "%.2f", rpm);
                    sprintf(spdHolder, "%d", high0);

                    strcpy(line2, "Spd:");
                    strncat(line2, spdHolder, 4);
                    strncat(line2, "  RPM:", 8);
                    strncat(line2, rpmHolder, 5);
                } else if (high1 > 7){
                    P3_1 = 0;
                    P3_0 = 1;       // we are moving clockwise

                    strcpy(line1, "Clockwise");
                    LCDprint(line1, 1, 1);

                    if (high1 > 100){
                        high1 = 100;
                    }
                    rpm = getRPM(high1);

                    sprintf(rpmHolder, "%.2f", rpm);
                    sprintf(spdHolder, "%d", high1);
                    strcpy(line2, "Spd:");
                    strncat(line2, spdHolder, 4);
                    strncat(line2, "  RPM:", 8);
                    strncat(line2, rpmHolder, 5);
                } else {
                    P3_1 = 1;       // LED inverted
                    P3_0 = 1;       // LED is inverted
                    strcpy(line1, "Static");
                    strcpy(line2, "Spd:0  RPM:0.00");
                }
                LCDprint(line1, 1, 1);
                LCDprint(line2, 2, 1);
            }
            printf("\nManual control deactivated.\n");
        } else {
            if (directionInt == 0){
                OUT0 = 0;
                OUT1 = 0;
                high0 = 0;
                high1 = 0;

                P3_1 = 1;       // LED inverted
                P3_0 = 1;       // LED is inverted

                strcpy(line1, "Static");
                strcpy(line2, "Spd:0  RPM:0.00");

            } else if (directionInt < 0){
                OUT0 = 1;
                OUT1 = 0;
                high0 = 50;
                high1 = 0;

                P3_1 = 1;       // we are moving ccw
                P3_0 = 0;       // LED is inverted

                strcpy(line1, "Anti-Clockwise");
                strcpy(line2, "Spd:50  RPM:24.29");
                LCDprint(line1, 1, 1);
                LCDprint(line2, 2, 1);

                printf("\nInput anti-clockwise speed: (0 - 100): ");
                getsn(buff1, sizeof(buff1));
                printf("\n");

                high0 = atoi(buff1);
                if (high0 > 100){
                    high0 = 100;
                }
                if (high0 <= 7){
                    strcpy(line1, "Static");
                    P3_0 = 1;
                    high0 = 0;
                }
                rpm = getRPM(high0);

                sprintf(rpmHolder, "%.2f", rpm);
                sprintf(spdHolder, "%d", high0);

                strcpy(line2, "Spd:");
                strncat(line2, spdHolder, 4);
                strncat(line2, "  RPM:", 8);
                strncat(line2, rpmHolder, 5);

            } else {
                OUT0 = 0;
                OUT1 = 1;
                high0 = 0;
                high1 = 50;

                P3_1 = 0;
                P3_0 = 1;       // we are moving clockwise

                strcpy(line1, "Clockwise");
                strcpy(line2, "Spd:50  RPM:24.29");
                LCDprint(line1, 1, 1);
                LCDprint(line2, 2, 1);

                printf("\nInput clockwise speed: (0 - 100): ");
                getsn(buff2, sizeof(buff2));
                printf("\n");

                high1 = atoi(buff2);
                if (high1 > 100){
                    high1 = 100;
                }
                if (high1 <= 7){
                    strcpy(line1, "Static");
                    P3_1 = 1;
                    high1 = 0;
                }
                rpm = getRPM(high1);

                sprintf(rpmHolder, "%.2f", rpm);
                sprintf(spdHolder, "%d", high1);
                strcpy(line2, "Spd:");
                strncat(line2, spdHolder, 4);
                strncat(line2, "  RPM:", 8);
                strncat(line2, rpmHolder, 5);
            }
        }

        LCDprint(line1, 1, 1);
        LCDprint(line2, 2, 1);
    }
}
