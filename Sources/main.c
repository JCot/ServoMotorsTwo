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
#define OP_CODE_MASK (0xE0)
#define PARAM_MASK (0x1F)
#define INITIALIZE (0xE0)

#define TIME_TO_MOVE (2); //Number of clock cycles it takes to move one position

// Change this value to change the frequency of the output compare signal.
// The value is in Hz.
#define OC_FREQ_HZ    ((UINT16)10)

// Macro definitions for determining the TC1 value for the desired frequency
// in Hz (OC_FREQ_HZ). The formula is:
//
// TC1_VAL = ((Bus Clock Frequency / Prescaler value) / 2) / Desired Freq in Hz
//
// Where:
//        Bus Clock Frequency     = 2 MHz
//        Prescaler Value         = 2 (Effectively giving us a 1 MHz timer)
//        2 --> Since we want to toggle the output at half of the period
//        Desired Frequency in Hz = The value you put in OC_FREQ_HZ
//
#define BUS_CLK_FREQ  ((UINT32) 2000000)   
#define PRESCALE      ((UINT16)  2)         
#define TC1_VAL       ((UINT16)  (((BUS_CLK_FREQ / PRESCALE) / 2) / OC_FREQ_HZ))

const unsigned int recipeOne[] = {
     MOV|0x00,
     MOV|0x05,
     MOV|0x00,
     RECIPE_END
};
  
const unsigned int recipeTwo[] = {
     MOV|0x03,
     LOOP_START|0x00,
     MOV|0x01,
     MOV|0x04,
     END_LOOP,
     MOV|0x01,
     RECIPE_END
};
  
const unsigned int recipeThree[] = {
     MOV|0x02,
     WAIT|0x00,
     MOV|0x03,
     RECIPE_END
};
  
const unsigned int recipeFour[] = {
     MOV|0x02,
     MOV|0x03,
     WAIT|0x1F,
     WAIT|0x1F,
     WAIT|0x1F,
     MOV|0x04,
     RECIPE_END
};

const unsigned int recipeFive[] = {
     MOV|0x02,
     MOV|0x00,
     MOV|0x04,
     MOV|0x01,
     MOV|0x03,
     MOV|0x05,
     RECIPE_END
};

const unsigned int recipeSix[] = {
     MOV|0x01,
     WAIT|0x01,
     MOV|0x03,
     RECIPE_END,
     MOV|0x04     
};

const unsigned int recipeSeven[] = {
     MOV|0x02,
     MOV|0x04,
     0x63,
     MOV|0x05,
     RECIPE_END
};

const unsigned int testRecipe[] = {
     MOV|0x02,
     RECIPE_END
};

const unsigned int InitializeRecipe[] = {
	INITIALIZE,
	RECIPE_END	
};

// Value to set PWMDTY0 to for the different positions
int dutyPositions[] = {2, 3, 4, 5, 6, 7};
//unsigned int leftRecipe = testRecipe;
//unsigned int rightRecipe = testRecipe;
int leftIndex = 0;
int rightIndex = 0;
int leftRecipeFinished = 0;
int rightRecipeFinished = 0;
int leftPosition = 0;
int rightPosition = 0;
int leftTimeToMove = 0;
int rightTimeToMove = 0;
int leftNumLoop = 0;
int rightNumLoop = 0;
int leftIndexLoop = 0;
int rightIndexLoop = 0;
char leftCommand = '\0';
char rightCommand = '\0';

int LeftState = STATE_INIT;
int RightState = STATE_INIT;

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
  int i = 0;
  int bigNumber = 35000;
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
  
  // Enable Pulse Width Channel One
  PWME_PWME0 = 1;
  PWME_PWME1 = 1;
   
  //
  // Enable interrupts via macro provided by hidef.h
  //
  //EnableInterrupts;
}




void InitializeTimer(void)
{
  // Set the timer prescaler to %4, since the bus clock is at 2 MHz,
  // and we want the timer running at 500 KHz
  TSCR2_PR0 = 0;
  TSCR2_PR1 = 1;
  TSCR2_PR2 = 0;
  
  // Enable output compare on Channel 1
  TIOS_IOS1 = 1;
  
  // Set up output compare action to toggle Port T, bit 1
  TCTL2_OM1 = 0;
  TCTL2_OL1 = 1;
  
  // Set up timer compare value
  TC1 = TC1_VAL;
  
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
void executeCommand(unsigned int cmd, int servo){
  //int index = 0;
  //int cmd = 0;
  int opCode = 0;
  int param = 0;
  
  //do{
    //cmd = recipe[index];
    opCode = cmd & OP_CODE_MASK;
    param = cmd & PARAM_MASK;
    
    //TODO: Trigger each command from interupt
    if(servo == 1){
    	
    	switch(opCode){
    		case MOV:{
        		int position = dutyPositions[(int)param]; //Position to move to.
        
        	 	leftTimeToMove = abs(position - leftPosition) * TIME_TO_MOVE;
        		leftPosition = (int)param;
        		PWMDTY0 = position;
        
        		break;
      		}
      		case WAIT:{
        		break;  
      		}
      		case LOOP_START:{
        		leftNumLoop = (int)param;
        		leftIndexLoop = leftIndex;
        
       			break;  
      		}
      		case END_LOOP:{
        		if(leftNumLoop > 0){
        
        			leftIndex = leftIndexLoop + 1;
            	
        			leftNumLoop--;
        		}
        	
        		break;
      		}
    		case INITIALIZE:{
    			PWMDTY0 = dutyPositions[5];
    			leftTimeToMove = 10;
    			PWMDTY0 = dutyPositions[0];
    		}
      		case RECIPE_END:{
      			
      			leftRecipeFinished = 1;
      			LeftState = STATE_RECIPE_END;
      			
      			leftRecipeFinished = 0;
        		leftIndex = 0;	
      			
        		break;    
      		}
    	}
    	
    	if(LeftState == STATE_RUN){
    		leftIndex++;
    	}
    }
    
    else{
    	switch(opCode){
    		case MOV:{
        		int position = dutyPositions[(int)param]; //Position to move to.
        
        		if(rightPosition > 5){
        			RightState = STATE_ERROR;	
        		}
        
        	 	rightTimeToMove = abs(position - rightPosition) * TIME_TO_MOVE;
        		rightPosition = (int)param;
        		PWMDTY1 = position;
        
        		break;
      		}
      		case WAIT:{
        		break;  
      		}
      		case LOOP_START:{
        		rightNumLoop = (int)param;
        		
        		rightIndexLoop = rightIndex;
        		
        
       			break;  
      		}
      		case END_LOOP:{
        		if(rightNumLoop > 0){
        
        			rightIndex = rightIndexLoop + 1;
        			
        			rightNumLoop--;
        		}
        	
        		break;
      		}
      		case RECIPE_END:{
      			
      			rightRecipeFinished = 1;
      			RightState = STATE_RECIPE_END;
      			
      			rightRecipeFinished = 0;
        		rightIndex = 0;
      			
        		break;    
      		}
    	}
    	
    	if(RightState == STATE_RUN){
    		rightIndex++;
    	}
    }
    
  //}while(cmd != RECIPE_END);
}

void executeRecipeStep(){
	int leftCmd;
	int rightCmd;
	
	if(LeftState == STATE_RUN){
		if(!leftRecipeFinished && leftTimeToMove == 0){
			leftCmd = InitializeRecipe[leftIndex];
		
			executeCommand(leftCmd, 1);	
		}
		
		else if(leftTimeToMove != 0){
			leftTimeToMove--;	
		}
	}
	
	if(RightState == STATE_RUN){
		if(!rightRecipeFinished && rightTimeToMove == 0){
			rightCmd = recipeFive[rightIndex];
		
			executeCommand(rightCmd, 2);
		}
		
		else if(rightTimeToMove != 0){
			rightTimeToMove--;	
		}
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
	executeRecipeStep();
	
	TFLG1 = TFLG1_C1F_MASK;      
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

void processInput(void){

	switch(leftCommand) {
        case 'l':
        case 'L': {
          
          	if(LeftState == STATE_PAUSE){
            	if(leftPosition < 5){
            		PWMDTY0 = dutyPositions[leftPosition + 1];
            		
            		leftPosition++;
            	}
          	}
        	break;     
        }
        case 'r':
        case 'R':{
        	if(LeftState == STATE_PAUSE){
        		if(leftPosition > 0){
           			PWMDTY0 = dutyPositions[leftPosition - 1];
                	
            		leftPosition--;
        		}
        	}
            
            break;
        }
        case 'p':
        case 'P':{
        
        	if(LeftState == STATE_RUN){
        		LeftState = STATE_PAUSE;
        	}
        	
            break; 
        }
        case 'c':
        case 'C':{
        
        	if(LeftState == STATE_PAUSE || LeftState == STATE_INIT){
        		LeftState = STATE_RUN;	
        	}
            
        	break; 
        }
        case 'n':
        case 'N':{
        	break; 
        }
        case 'b':
        case 'B':{
        
        	if(LeftState == STATE_PAUSE || LeftState == STATE_INIT
        	   || LeftState == STATE_ERROR || LeftState == STATE_RECIPE_END){
	
        		LeftState = STATE_RUN;
        	}
        
        	break; 
        } 
          
        default:{
        	break;
    	}
          
	};
	
	switch(rightCommand) {
        case 'l':
        case 'L': {
          
            if(RightState == STATE_PAUSE){
            	if(rightPosition < 5){
            		PWMDTY1 = dutyPositions[rightPosition + 1];
            		
            		rightPosition++;
            	}
            }
        	break;     
        }
        case 'r':
        case 'R':{
        	if(RightState == STATE_PAUSE){
        		if(rightPosition > 0){
            		PWMDTY1 = dutyPositions[rightPosition - 1];
              
            		rightPosition--;
        		}
        	}
            
            break;
        }
        case 'p':
        case 'P':{
        
        	if(RightState == STATE_RUN){
        		RightState = STATE_PAUSE;
        	}
        
            break; 
        }
        case 'c':
        case 'C':{
            
            if(RightState == STATE_PAUSE || RightState == STATE_INIT){
        		RightState = STATE_RUN;	
        	}
            
        	break; 
        }
        case 'n':
        case 'N':{
        	break; 
        }
        case 'b':
        case 'B':{
        
        	if(RightState == STATE_PAUSE || RightState == STATE_INIT
        	   || RightState == STATE_ERROR || RightState == STATE_RECIPE_END){
        		
        		RightState = STATE_RUN;	
        	}
        	
        	break; 
        } 
          
        default:{
        	break;
    	}
          
	};
	
	leftCommand = '\0';
	rightCommand = '\0';		
}


// Polls for a character on the serial port.
// Need to change
// Returns: Received character
//--------------------------------------------------------------       
void GetChar(void)
{ 
   //Poll for data
  //do
  //{
    // Nothing
  //} while(SCI0SR1_RDRF == 0);
  
 if(SCI0SR1_RDRF != 0){
 	if(SCI0DRL == 'x' || SCI0DRL == 'X'){
 		leftCommand = '\0';
 		rightCommand = '\0';
 		(void)printf("\n\r>");
 	}
 	
 	else if(leftCommand == '\0'){
 		leftCommand = SCI0DRL;
 		TERMIO_PutChar(leftCommand);
 	}
 	
 	else if(rightCommand == '\0'){
 		rightCommand = SCI0DRL;	
 		TERMIO_PutChar(rightCommand);
 	}
 	
 	else if(SCI0DRL == '\r'){
 		DisableInterrupts;
 		processInput();
 		(void)printf("\r\n>");
 		EnableInterrupts;
 	}
 }
}

void main(void) {
  /* put your own code here */ 
  int ServoIndex=0;
  InitializeSerialPort();
  Initialize();
  InitializeTimer();
       

  while(1 == 1) {
  
        //char temp= GetChar();
        GetChar();
        
      
  } /* loop forever */
}
