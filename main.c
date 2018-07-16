

#include "msp430.h"


#define     LED1                  BIT0
#define     LED2                  BIT6

#define     BUTTON                BIT3

#define     BEEP                  BIT2

#define     STROBE                BIT5

#define     MODE                  BIT4
#define     SHUTTER               BIT7

#define     PreAppMode            0
#define     RunningMode           1

#define     Threshold             0080

volatile unsigned int Mode;
volatile unsigned int High;
volatile unsigned int BAC;
  
void InitializeButton(void);
void PreApplicationMode(void);

void DisplayNum(int num);
void takePhoto(void);


void main(void)
{
    WDTCTL = WDTPW + WDTHOLD;                       // Stop WDT

    // ADC10 on and choose input P1.1 ("A1")
    ADC10CTL0 = ADC10SHT_2 + ADC10ON + ADC10IE; 	// turns on the ADC, interrupts enabled
    ADC10CTL1 = INCH_1;                             // input A1 (AKA p1.1)
    ADC10AE0 |= BIT1;                               // PA.1 ADC option select

    InitializeButton();                             // set up button to interrupt preappmode

    // set up ports for leds:
    P1DIR |= LED1 + LED2;                          
    P1OUT &= ~(LED1 + LED2);

    // set up ports for ADC display
    P2DIR = 63;
    P1DIR |= STROBE;

    Mode = PreAppMode;
    PreApplicationMode();                           // flash lights and wait for button press

    __enable_interrupt();

    for (;;)
    {

        High = 0;                                   // lower-than-threshold alcohol level is assumed at the start of the main loop; this may change upon measurement
        P1OUT |= LED2;

        ADC10CTL0 |= ENC + ADC10SC;                 // ADC sampling and conversion start
        __bis_SR_register(CPUOFF + GIE);            // LPM0 with interrupts enabled; turn cpu off.
        // an interrupt is triggered when the ADC result is ready. 
        // the interrupt handler performs one of two tasks based on air alcohol concentration, then restarts the cpu

        __delay_cycles(1000);                       // wait for ADC Ref to settle

        if(High == 1)                               // if more than the threshold was detected
        {
            InitializeButton();
            Mode = PreAppMode;
            PreApplicationMode();                   // return to preappmode and wait for button press
        }
        else
        {
            __delay_cycles(500000);                 // wait half a second between measurements

        }

    }
}


/************************************SUBROUTINES***********************************/


// toggles LEDs in low power mode until interrupted by button press
// from TI's MSP430G2553 temperature demo program
void PreApplicationMode(void)
{
    P1OUT |= LED1;                                  // To enable the LED toggling effect
    P1OUT &= ~LED2;

    // configure ACLK (not using SMCLK because SMCLK can't run in LMP3 but ACLK can)
    BCSCTL1 |= DIVA_1;                              // ACLK is half the speed of the source (VLO)
    BCSCTL3 |= LFXT1S_2;                            // ACLK = VLO

    // make timer interrupt periodically; ISR will toggle lights
    TACCR0 = 1200;                                  // period
    TACTL = TASSEL_1 | MC_1;                        // TACLK = ACLK, Up mode.  
    TACCTL1 = CCIE + OUTMOD_3;                      // TACCTL1 Capture Compare
    TACCR1 = 600;                                   // duty cycle
    __bis_SR_register(LPM3_bits + GIE);             // LPM3 with interrupts enabled
}


// configure push button: enable pullup resistor and interrupts on port 3
// from TI's MSP430G2553 temperature demo program
void InitializeButton(void)
{
    P1DIR &= ~BUTTON;
    P1OUT |= BUTTON;                                // this will set internal resistor as pull-up
    P1REN |= BUTTON;                                // enable internal resistor
    P1IES |= BUTTON;                                // P1IFG flag chosen to trigger under a HIGH-to-LOW transition!
    P1IFG &= ~BUTTON;                               // clear interrupt flag
    P1IE |= BUTTON;                                 // enable interrupts
}


// takes an integer as input and displays its 4 least significant digits on the 7-segment displays
void DisplayNum(int num)
{
    int d[4] = {0,0,0,0};                           // initialize an array to hold the digits of num, from least significant to most significant
    int i = 0;
    while(num > 0)
    {
        d[i] = num % 10;                            // will return the least significant digit
        num = (num-(num % 10))/10;                  // equivalent to num = floor(num/10), but our makefile doesn't understand floor()
        i++;                                        // continue to iterate through num
    }
    
    // change position 0 on display
    P1OUT |= STROBE;                                // d[0], num's least significant digit, is a one-digit decimal number and a 4-digit binary number
    P2OUT = d[0] + 0;                               // the four binary digits are stored in P2.0-P2.3; P2.4 and P2.5 control A0 and A1, respectively
    P1OUT &= ~STROBE;                               // in this case, A0 = A1 = 0 means we're selecting position 0, which is the rightmost 7-segment display

    // change position 1 on display
    P1OUT |= STROBE;
    P2OUT = d[1] + 16;                              // here, P2OUT = 00(D3)(D2)(D1)(D0) + 010000, where D0-3 are the binary digits of d[1] and 010000b = 16
    P1OUT &= ~STROBE;                               // this means that the number (D3)(D2)(D1)(D0) = d[1] is displayed at position (A1)(A0) = 01b = 1

    // change position 2 on display
    P1OUT |= STROBE;
    P2OUT = d[2] + 32;                              // similiarly, here 32 = 100000b, so P2OUT = 32 + d[2] plots d[2] on position (A1)(A0) = (1)(0) = 2
    P1OUT &= ~STROBE;

    // change position 3 on display
    P1OUT |= STROBE;
    P2OUT = d[3] + 48;                              // 48 = 110000b
    P1OUT &= ~STROBE;
}


// turns on the camera, takes a single photo, then turns off the camera
void takePhoto(void)
{
    // set up ports for camera
    P1DIR |= SHUTTER + MODE;

    // button combinations for accessing different modes of the camera can be found in the operation manual
    // here, we just employ the on and off button sequences

    // turn camera on
    P1OUT |= MODE;
    __delay_cycles(250000);
    P1OUT &= ~MODE;
    __delay_cycles(1000000);

    // take picture
    P1OUT |= SHUTTER;
    __delay_cycles(250000);
    P1OUT &= ~SHUTTER;
    __delay_cycles(4000000);

    // turn camera off
    P1OUT |= MODE;
    __delay_cycles(250000);
    P1OUT &= ~MODE;
    __delay_cycles(250000);
    P1OUT |= SHUTTER;
    __delay_cycles(250000);
    P1OUT &= ~SHUTTER;
    __delay_cycles(1000000);

}

/***************************************ISRS***************************************/


// responds to button press by switching to running mode; uses WDT interrupt to de-bounce button
// from TI's MSP430G2553 temperature demo program
#if defined(__TI_COMPILER_VERSION__)
#pragma vector=PORT1_VECTOR
__interrupt void port1_isr(void)
#else
  void __attribute__ ((interrupt(PORT1_VECTOR))) port1_isr (void)
#endif
{   
    P1IFG = 0;                                      // clear interrupt flag
    P1IE &= ~BUTTON;                                // Disable port 1 interrupts 
    WDTCTL = WDT_ADLY_250;                          // set up watchdog timer duration 
    IFG1 &= ~WDTIFG;                                // clear interrupt flag 
    IE1 |= WDTIE;                                   // enable watchdog interrupts
    
    TACCTL1 = 0;                                    // turn off timer 1 interrupts
    P1OUT &= ~(LED1+LED2);                          // turn off the LEDs
    Mode = RunningMode;

    __bic_SR_register_on_exit(LPM3_bits);           // take us out of low power mode
}


// WDT interrupt to de-bounce button press; prevents MSP from responding to two button presses close together
// from TI's MSP430G2553 temperature demo program
#if defined(__TI_COMPILER_VERSION__)
#pragma vector=WDT_VECTOR
__interrupt void wdt_isr(void)
#else
  void __attribute__ ((interrupt(WDT_VECTOR))) wdt_isr (void)
#endif
{
    IE1 &= ~WDTIE;                                  // disable watchdog interrupt
    IFG1 &= ~WDTIFG;                                // clear interrupt flag
    WDTCTL = WDTPW + WDTHOLD;                       // put WDT back in hold state
    P1IE |= BUTTON;                                 // eenable port 1 interrupts
}


// toggle lights if in preappmode; clear interrupts and turn CPU back on if not
// from TI's MSP430G2553 temperature demo program
#if defined(__TI_COMPILER_VERSION__)
#pragma vector=TIMER0_A1_VECTOR
__interrupt void ta1_isr (void)
#else
  void __attribute__ ((interrupt(TIMER0_A1_VECTOR))) ta1_isr (void)
#endif
{
    TACCTL1 &= ~CCIFG;                              // reset interrupt flag
    if (Mode == PreAppMode){
      P1OUT ^= (LED1 + LED2);                       // toggle LEDs
    }
    else{
      TACCTL1 = 0;                                  // prevent further interrupts
      __bic_SR_register_on_exit(LPM3_bits);         // turn CPU back on
    }
    
}


// ADC10 interrupt service routine; calculates blood alcohol concentration from ADC result when ADC is ready
#if defined(__TI_COMPILER_VERSION__)
#pragma vector=ADC10_VECTOR
__interrupt void adc10_isr(void)
#else
  void __attribute__ ((interrupt(ADC10_VECTOR))) adc10_isr (void)
#endif
{

    // calculates BAC from ADC if ADC at base reading; if sensor voltage is fluctuating below base reading, it returns zero
    if(ADC10MEM > 100)
    {
        BAC = (int) 1000*(ADC10MEM-100)/2750;
    }
    else
    {
        BAC = 0;
    }

    // update 7-segment display
    DisplayNum(BAC);

    if (BAC < Threshold)
    {
        High = 0;                                   // above-threshold indicator remains low
        P1OUT |= LED2;                              // green on if it wasn't already
        __bic_SR_register_on_exit(CPUOFF);          // do nothing if no significant amt of alcohol
    }
    else
    {
        High = 1;                                   // above-threshold indicator high
        P1OUT &= ~(LED2);                           // green off
        P1OUT |= LED1;                              // red on if it wasn't already

        // Beep
        P1DIR |= BEEP; 				                // P1.2 to output
        P1SEL |= BEEP; 				                // P1.2 to TA0.1

        CCR0 = 800; 				                // PWM Period
        CCTL1 = OUTMOD_7;			                // CCR1 reset/set
        CCR1 = 420; 				                // CCR1 PWM duty cycle
        TACTL = TASSEL_2 + MC_1; 		            // SMCLK, up mode
        // Beep

        takePhoto();
        
        __delay_cycles(500000);                     // wait half a second between measurements

        P1DIR &= ~BEEP;                             // disable PWM signal

        __bic_SR_register_on_exit(CPUOFF);          // turn CPU back on
    }
}

