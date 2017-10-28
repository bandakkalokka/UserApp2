/* 
 * File:   main.c
 * Author: rv
 *
 * Created on January 9, 2017, 5:26 PM
 */


#include "xc.h"
//#include <p24fxxxx.h>
#include <p24F16KA101.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include "Timer2.h"
#include "Timer1.h"
#include "States.h"
#include "IO.h"
#include "UART2.H"
#include "Output-Compare.h"

#include "ChangeClk.h"

//// CONFIGURATION BITS ////

// Code protection 
#pragma config BSS = OFF // Boot segment code protect disabled
#pragma config BWRP = OFF // Boot sengment flash write protection off
#pragma config GCP = OFF // general segment code protecion off
#pragma config GWRP = OFF

// CLOCK CONTROL 
#pragma config IESO = OFF    // 2 Speed Startup disabled
#pragma config FNOSC = FRC  // Start up CLK = 8 MHz
#pragma config FCKSM = CSECMD // Clock switching is enabled, clock monitor disabled
#pragma config SOSCSEL = SOSCLP // Secondary oscillator for Low Power Operation
#pragma config POSCFREQ = MS  //Primary Oscillator/External clk freq betwn 100kHz and 8 MHz. Options: LS, MS, HS
#pragma config OSCIOFNC = ON  //CLKO output disabled on pin 8, use as IO. 
#pragma config POSCMOD = NONE  // Primary oscillator mode is disabled

// WDT
#pragma config FWDTEN = OFF // WDT is off
#pragma config WINDIS = OFF // STANDARD WDT/. Applicable if WDT is on
#pragma config FWPSA = PR32 // WDT is selected uses prescaler of 32
#pragma config WDTPS = PS1 // WDT postscler is 1 if WDT selected

//MCLR/RA5 CONTROL
#pragma config MCLRE = OFF // RA5 pin configured as input, MCLR reset on RA5 diabled

//BOR  - FPOR Register
#pragma config BORV = LPBOR // LPBOR value=2V is BOR enabled
#pragma config BOREN = BOR0 // BOR controlled using SBOREN bit
#pragma config PWRTEN = OFF // Powerup timer disabled
#pragma config I2C1SEL = PRI // Default location for SCL1/SDA1 pin

//JTAG FICD Register
#pragma config BKBUG = OFF // Background Debugger functions disabled
#pragma config ICS = PGx2 // PGC2 (pin2) & PGD2 (pin3) are used to connect PICKIT3 debugger

// Deep Sleep RTCC WDT
#pragma config DSWDTEN = OFF // Deep Sleep WDT is disabled
#pragma config DSBOREN = OFF // Deep Sleep BOR is disabled
#pragma config RTCOSC = LPRC// RTCC uses LPRC 32kHz for clock
#pragma config DSWDTOSC = LPRC // DeepSleep WDT uses Lo Power RC clk
#pragma config DSWDTPS = DSWDTPS7 // DSWDT postscaler set to 32768 


// GLOBAL VARIABLES
unsigned int temp;
unsigned int i;
volatile unsigned int timer_state = 1;
volatile ButtonPress ButtonPressed;     // Variable indicates which button was pressed. 0 - None. 1 - PB1. 2 - PB2. 3 - Both.
volatile TvOperation tvState;           // Variable indicated the current operation mode of the TV
volatile unsigned int CNFlag;

// MACROS
#define Nop() {__asm__ volatile ("nop");}
#define ClrWdt() {__asm__ volatile ("clrwdt");}
#define Sleep() {__asm__ volatile ("pwrsav #0");}   // set sleep mode
#define Idle() {__asm__ volatile ("pwrsav #1");}
#define dsen() {__asm__ volatile ("BSET DSCON, #15");}


int main(void) {
    //Clock output on REFO
    TRISBbits.TRISB15 = 0;  // Set RB15 as output for REFO
    REFOCONbits.ROEN = 1; // Ref oscillator is enabled
    REFOCONbits.ROSSLP = 0; // Ref oscillator is disabled in sleep
    REFOCONbits.ROSEL = 0; // Output base clk showing clock switching
    REFOCONbits.RODIV = 0b0000;
     
    // Switch clock: 32 for 32kHz, 500 for 500 kHz, 8 for 8MHz 
    NewClk(8); //
    InitTimer2();
    InitTimer1();
    InitCN();
    TRISBbits.TRISB8 = 0;           // Configured as output PWM pin
    TRISAbits.TRISA4 = 1;           // Configured as input button PB2
    TRISBbits.TRISB4 = 1;           // Configured as input button PB1
    TRISAbits.TRISA3 = 0;           // Configured as output LED (Indicate Volume/Channel mode change)
    LATAbits.LATA3 = 0;
    LATBbits.LATB8 = 0;
 
    
    tvState = IDLE;
    ButtonPressed = NONE;
    CNFlag = 0;
    main_timer_done = 0;
    
    
    while(1)
    {
        switch (tvState)
        {
            case IDLE:
                if (CNFlag)
                { 
                  delay_ms(100);
                  PollCN();
                  CNFlag = 0;
                  if (ButtonPressed == BOTH)
                  {
                    tvState = POWER_ON;
                    ButtonPressed = NONE;
                  }
                }
                else {
                    Sleep();
                }
                break;
      
            case POWER_ON:
                //DispString("Powering On!");
                IEC1bits.CNIE = 0;
                PowerOn();                // Emit power on signal
                //delay_ms(200);            // Delay for some time to allow TV to turn on. This is an initial guess
                tvState = CHANNEL_MODE;   // Change to channel mode
                IEC1bits.CNIE = 1;
                CNFlag = 0;
                break;
      
            case POWER_OFF:
                //DispString("Powering Off");
                IEC1bits.CNIE = 0;
                PowerOff();               // Emit power off signal
                delay_ms(500);            // Delay for some time to allow TV to turn off. This is an initial guess
                tvState = IDLE;            // Change to idle state
                IEC1bits.CNIE = 1;
                CNFlag = 0;
                break;
      
            case CHANNEL_MODE:
                //DispString("Channel Mode");
                if (CNFlag)
                {
                    PollCN();

                  if (ButtonPressed == UP)
                  {
                    Change_Channel(0);
                  }
                  else if (ButtonPressed == DOWN)
                  {
                    Change_Channel(1);
                  }
                  else if(ButtonPressed == BOTH) {
                      delay_ms(80);
                      PollLength();
                  }
                }
                else
                {
                  Idle();
                }
                break;
            
            case VOLUME_MODE:
                //DispString("Volume Mode");
              if (CNFlag)
              {
                 PollCN();

                if (ButtonPressed == UP)
                {
                  Change_Volume(0);
                }
                else if (ButtonPressed == DOWN)
                {
                  Change_Volume(1);
                }
                else if(ButtonPressed == BOTH) {
                    delay_ms(80);
                    PollLength();
                }
              }
                else
                {
                  Idle();
                }
            break;
      
            default:
                tvState = POWER_OFF;
    }

  }
    return 0;
}

