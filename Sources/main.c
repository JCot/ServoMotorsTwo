                                            #include <hidef.h>      /* common defines and macros */
#include "derivative.h"      /* derivative-specific definitions */
#include "types.h"
#include "State.h"
#include <stdio.h>

// Definitions

// Change this value to change the frequency of the output compare signal.
// The value is in Hz.
#define OC_FREQ_HZ    ((UINT16)10)

//Define commands
#define MOV (0x20)
#define WAIT (0x40)
#define LOOP_START (0x80)
#define END_LOOP (0xA0)
#define RECIPE_END (0x00)
#define CMD_MASK (0xE0)

const unsigned char recipeOne[] = {
     MOV|0x00,
     MOV|0x05,
     MOV|0x00
};
  
const unsigned char recipeTwo[] = {
     MOV|0x03,
     LOOP_START|0x00,
     MOV|0x01,
     MOV|0x04,
     END_LOOP,
     MOV|0x01
};
  
const unsigned char recipeThree[] = {
     MOV|0x02,
     WAIT|0x00,
     MOV|0x03
};
  
const unsigned char recipeFour[] = {
     MOV|0x02,
     MOV|0x03,
     WAIT|0x1F,
     WAIT|0x1F,
     WAIT|0x1F,
     MOV|0x04
};

const unsigned char recipeFive[] = {
     MOV|0x00,
     MOV|0x02,
     MOV|0x04,
     MOV|0x01,
     MOV|0x03,
     MOV|0x05
};

const unsigned char recipeSix[] = {
     MOV|0x01,
     WAIT|0x01,
     RECIPE_END,
     MOV|0x03     
};

const unsigned char recipeSeven[] = {
     MOV|0x02,
     MOV|0x04,
     0x63,
     MOV|0x05
};

// Value to set PWMDTY0 to for the different positions
int dutyPositions[] = {2, 3, 4, 5, 6, 7};
int dutyIndex = 0;

// Initializes SCI0 for 8N1, 9600 baud, polled I/O
// The value for the baud selection registers is determined
// using the formula:
//
// SCI0 Baud Rate = ( 2 MHz Bus Clock ) / ( 16 * SCI0BD[12:0] )
//--------------------------------------------------------------
void InitializeSerialPort(void)
{
    // Set baud rate to ~9600 (See above formula)
    SCI0BD = 13;          
    
    // 8N1 is default, so we don't have to touch SCI0CR1.
    // Enable the transmitter and receiver.
    SCI0CR2_TE = 1;
    SCI0CR2_RE = 1;
}

void Initialize(void)
{
  // Set the timer prescaler to %128, and the Scale A register to %78 
  // since the bus clock is at 2 MHz,and we want the timer 
  // running at 50 Hz
  PWMCLK_PCLK0 = 1;
  PWMCLK_PCLK1 = 1;
  PWMPRCLK_PCKA0 = 1;
  PWMPRCLK_PCKA1 = 1;
  PWMPRCLK_PCKA2 = 1;
  PWMSCLA = 2;
  PWMPER0 = 80;
  PWMDTY0 = 2;
  PWMPER1 = 80;
  PWMDTY1 = 2;
  
  // Set the Polarity bit to one to ensure the beginning of the cycle is high
  PWMPOL = 3;
  
  // Enable Pulse Width Channel One, Move once we begin to move servos
  PWME_PWME0 = 1;
  PWME_PWME1 = 1;
   
  //
  // Enable interrupts via macro provided by hidef.h
  //
  //EnableInterrupts;
}




void InitializeTimer(void)
{
  // Set the timer prescaler to %2, since the bus clock is at 2 MHz,
  // and we want the timer running at 1 MHz
  TSCR2_PR0 = 1;
  TSCR2_PR1 = 1;
  TSCR2_PR2 = 1;
  
  // Set up timer compare value
  TC1 = TCNT;
  
  // Clear the Output Compare Interrupt Flag (Channel 1) 
  TFLG1 = TFLG1_C1F_MASK;
  
  // Enable the output compare interrupt on Channel 1;
  TIE_C1I = 1;  
  
  //
  // Enable the timer
  // 
  TSCR1_TEN = 1;
   
  //
  // Enable interrupts via macro provided by hidef.h
  //
  EnableInterrupts;
}

//Pass numElements as sizeof(array)/sizeof(array[0])
void readRecipe(unsigned char recipe[], int numElements, int servo){
  int index = 0;
  
  while(index < numElements){
    char cmd = recipe[index];
    char opCode = cmd & CMD_MASK;
    char param = cmd & opCode;
    
    //TODO: Trigger each command from interupt
    switch(opCode){
      case MOV:{
        int position = dutyPositions[(int)param];
        
        if(servo == 1){
          PWMDTY0 = position; 
        } else{
          PWMDTY1 = position;
        }
      }
      case WAIT:{
        
      }
      case LOOP_START:{
        
      }
      case END_LOOP:{
        
      }
      case RECIPE_END:{
        
      }
    }
    
    index++;  
  }
}


// Output Compare Channel 1 Interrupt Service Routine
// Refreshes TC1 and clears the interrupt flag.
//          
// The first CODE_SEG pragma is needed to ensure that the ISR
// is placed in non-banked memory. The following CODE_SEG
// pragma returns to the default scheme. This is neccessary
// when non-ISR code follows. 
//
// The TRAP_PROC tells the compiler to implement an
// interrupt funcion. Alternitively, one could use
// the __interrupt keyword instead.
// 
// The following line must be added to the Project.prm
// file in order for this ISR to be placed in the correct
// location:
//		VECTOR ADDRESS 0xFFEC OC1_isr 
#pragma push
#pragma CODE_SEG __SHORT_SEG NON_BANKED
//--------------------------------------------------------------       
void interrupt 9 OC1_isr( void )
{     
    
}
#pragma pop

// This function is called by printf in order to
// output data. Our implementation will use polled
// serial I/O on SCI0 to output the character.
//
// Remember to call InitializeSerialPort() before using printf!
//
// Parameters: character to output
//--------------------------------------------------------------       
void TERMIO_PutChar(INT8 ch)
{
    // Poll for the last transmit to be complete
    do
    {
      // Nothing  
    } while (SCI0SR1_TC == 0);
    
    // write the data to the output shift register
    SCI0DRL = ch;
}


// Polls for a character on the serial port.
// Need to change
// Returns: Received character
//--------------------------------------------------------------       
UINT8 GetChar(void)
{ 
   //Poll for data
  do
  {
    // Nothing
  } while(SCI0SR1_RDRF == 0);
 //  
  // Fetch and return data from SCI0
  return SCI0DRL;
}

void wait() {
int index =0;
while(index<10000000) {
index++;
}
}

void main(void) {
  /* put your own code here */ 
  int ServoIndex=0;
  InitializeSerialPort();
  InitializeTimer();
	Initialize();
       

  while(1 == 1) {
  
        char temp= GetChar();
        ServoIndex ++;
        switch(temp) {
          case 'l':
          case 'L': {
          
            if(dutyIndex < 5){
           
              PWMDTY0 = dutyPositions[dutyIndex + 1];
              PWMDTY1 = dutyPositions[dutyIndex + 1];
              dutyIndex++;
            }
            break;     
          }
          case 'r':
          case 'R':{
           if(dutyIndex > 0){
              PWMDTY0 = dutyPositions[dutyIndex - 1];
              PWMDTY1 = dutyPositions[dutyIndex - 1];
              
              dutyIndex--;
           }
            break;
          }
          case 'p':
          case 'P':{
            break; 
          }
          case 'c':
          case 'C':{
            break; 
          }
          case 'n':
          case 'N':{
            break; 
          }
          case 'b':
          case 'B':{
            break; 
          }
          case 'x':
          case 'X':{
            break; 
          } 
          
          default:{
            break;
          }
          
        }
      
  } /* loop forever */
}
