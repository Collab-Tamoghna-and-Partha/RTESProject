/* 	File: 		main.c
* 	Author: 	TAMOGHNA CHAKRABORTY&PARTHA PRATIM NATH
*/

/* system imports */
#include <mbed.h>
#include <math.h>
#include <USBSerial.h>

/* user imports */
#include "LIS3DSH.h"


/* USBSerial library for serial terminal */
USBSerial serial(0x1f00,0x2012,0x0001,false);

/* LIS3DSH Library for accelerometer  - using SPI*/
LIS3DSH acc(PA_7, SPI_MISO, SPI_SCK, PE_3);

/* LED output */
DigitalOut ledOut(LED1);
DigitalOut ledOut1(LED2);
DigitalOut ledOut4(LED5);
DigitalOut ledOut5(LED6);
InterruptIn button(USER_BUTTON);

/* variable initialisation */
int flag_situp=1, flag_pushup=0, flag_squat=0, flag_jj=0; //variables for exercise selection
float g_x = 0, g_y = 0, g_z=0, a_x=0, a_y=0, a_z=0; //nromalized accelerations('g' variables) and angles('a' variables) wrt relative axes
int c_SU = 0, c_PU = 0, c_SQ = 0, c_JJ = 0; //rep counters for each exercise

/* function for mode setting. When called, checks current mode and changes to next consecutive mode. Default mode: situp */

static void modeselect(){
	if(flag_situp==1){ //current mode situp, change to pushup
		flag_situp=0;
		flag_pushup=1;
		flag_squat=0;
		flag_jj=0;
	}
	else if(flag_pushup==1){ //current mode pushup, change to squats
		flag_situp=0;
		flag_pushup=0;
		flag_squat=1;
		flag_jj=0;
	}
	else if(flag_squat==1){ //current mode squats, change to jumping jacks
		flag_situp=0;
		flag_pushup=0;
		flag_squat=0;
		flag_jj=1;
	}
	else{ //current mode jumping jacks, change to situp
		flag_situp=1;
		flag_pushup=0;
		flag_squat=0;
		flag_jj=0;
	}
}

int main() {
	int16_t X, Y;							// accelerations in x and y axes (raw)
	int16_t zAccel = 0; 					// acceleration in z axis (raw)
	const float PI = 3.1415926;				// pi

	/**** Filter Parameters  ****/
    const uint8_t N = 50; 					// sample buffer length

	/*sample buffers */
    float ringbuf_x[N];
	float ringbuf_y[N];
	float ringbuf_z[N];	

    uint8_t ringbuf_index = 0;				// index to insert sample
	
	/* check detection of the accelerometer */
	while(acc.Detect() != 1) {
        printf("Could not detect Accelerometer\n\r");
       	wait_ms(200);
    }
	
	int p = 0;                              //variable used to slow down button polling rate 
	

	int state_SU = 0, state_PU = 0, state_SQ = 0, state_JJ = 0;    //binary state variables to determine up or down positions
	
	
	while(1) {
		wait_ms(100);
		/* condition to blink blue and orange LEDs after completion of all exercises */
		if(c_SU >= 15 && c_PU>=7 && c_SQ>=15 && c_JJ>=15)
		{
			ledOut=!ledOut;
			ledOut5=!ledOut5;
			continue;
		}
		/* condition for changing mode wrt button push */
		if((p % 10 ==0) && (button.read() == 1))  // polling button
		{
			modeselect();                    //changing mode
			p = 0;
		}
		p++;

		
		/* read data from the accelerometer */
		acc.ReadData(&X, &Y, &zAccel);

		/* insert in to circular buffer */

		/* Through experimentation, it was seen that the best classifiers were obtained from the 
		double differentiation of the samples stored in the 5 second buffer space.
		I noticed that the classifiers for each exercise were best spaced from each other when the raw values were reduce
		by a factor of 8000. */
		ringbuf_x[ringbuf_index] = X/8000;
		ringbuf_y[ringbuf_index] = Y/8000;
		ringbuf_z[ringbuf_index++] = zAccel/8000;
		/* at the end of the buffer, wrap around to the beginning */
		if (ringbuf_index >= N) {
			ringbuf_index = 0;
		}

		/* compute normalized accelerations and angles in degrees */
		g_z = (float)zAccel/17694.0;
		a_z = 180*acos(g_z)/PI;
		g_x = (float)X/17694.0;
		a_x = 180*acos(g_x)/PI;
		g_y = (float)Y/17694.0;
		a_y = 180*acos(g_y)/PI;

		int dx=0, dx1=0, cx=0, dy = 0, dy1 = 0, cy = 0, dz = 0, dz1 = 0, cz = 0;  //classifier variables for each exercise

		/* loop for calculating difference between successive sample spaces and counting the no of 
		times the difference changes from postive to negative and vice versa. This generates a variable proportional to the
		frequency of movement in each axis */ 

		for (uint8_t i = 0; i < N-1; i++)
		 {
			dx1 = ringbuf_x[i+1]-ringbuf_x[i];
			dy1 = ringbuf_y[i+1]-ringbuf_y[i];
			dz1 = ringbuf_z[i+1]-ringbuf_z[i];

			if ((dx<0 && dx1>=0)||(dx>=0 && dx1<0)){
				cx++;
			}
			if ((dy<0 && dy1>=0)||(dy>=0 && dy1<0)){
				cy++;
			}
			if ((dz<0 && dz1>=0)||(dz>=0 && dz1<0)){
				cz++;
			}
			dx=dx1;
			dy = dy1;
			dz = dz1;
		}

		/* conditional statements for detecting workouts*/

		if (flag_situp==1){
			 ledOut1=0; ledOut4=0;					//red and green LEDS to show various modes. For situp, both are off
			 if(c_SU >= 15)
			{
				ledOut5 = !ledOut5;					//blue LED to blink when 15 reps are completed
				continue;
			}
			 serial.printf("Mode: Situps: \t");
			
			if ((cx >= 8 && cx <= 18)&&(cy >= 0 && cy <= 2)&&(cz>= 10 && cz <= 25))  //condition to check if situps are being performed
			{
				ledOut = 0;                         //orange LED is off when exercise is being done
				ledOut5 = 1;						//blue LED is on when exercise is being done
				if(a_x < 50 )						//for counting no of reps
				{
					if(state_SU == 0)
					{
						c_SU++;
					}
					state_SU = 1;
				}
				else
				state_SU = 0;
				
				serial.printf("You are doing it right! Count: %d \n",c_SU);
			}
			else
			{
				ledOut = 1;                            //idle state. Orange LED is on
				ledOut5 = 0;
				serial.printf("You are doing it wrong! Count: %d\n",c_SU);
			}
		}
		if (flag_pushup==1){
			ledOut1=0; ledOut4=1;							// For pushup, only red LED glows
			if(c_PU >= 7)
			{
				ledOut5 = !ledOut5;							//blue LED to blink when 7 reps are completed
				continue;
			}
			serial.printf("Mode: Pushups: \t");
			
			if ((cx >= 0 && cx <= 2)&&(cy >= 0 && cy <= 2)&&(cz>= 10 && cz <= 18))  //condition to check if pushups are being performed
			{
				ledOut = 0;									//active state. Orange LED is off when exercise is being done
				ledOut5 = 1;								//blue LED is on when exercise is being done
				if(a_z > 100)								//for counting no of reps
				{
					if(state_PU == 0)
					{
						c_PU++;
					}
					state_PU = 1;
				}
				else
				{
					state_PU = 0;
				}
				
				serial.printf("You are doing it right! Count: %d \n", c_PU);
			}
			else
			{
				ledOut = 1;//idle
				ledOut5 = 0;
				serial.printf("You are doing it wrong! Count: %d \n",c_PU);
			}
		}
		if (flag_squat==1){
			ledOut1=1; ledOut4=0;								// For SQUATS, only green LED glows
			if(c_SQ>=15)
			{
				ledOut5 = !ledOut5;								//15 reps completed, blue LED blinks
				continue;
			}
			serial.printf("Mode: Squats: \t");
			if ((cx >= 14 && cx <= 27)&&(cy >= 0 && cy <= 2)&&(cz>= 4 && cz <= 13))  //active state
			{
				ledOut = 0;										
				ledOut5 = 1;
				if(a_z > 120 )										//counting reps
				{
					if(state_SQ == 0)
					{
						c_SQ++;
					}
					state_SQ = 1;
				}
				else      
				state_SQ = 0;
				serial.printf("You are doing it right! Count: %d \n", c_SQ);
			}
			else
			{
				ledOut = 1;						//idle state
				ledOut5 = 0;
				serial.printf("You are doing it wrong! Count: %d\n",c_SQ);
			}
		}
		if (flag_jj==1)
		{
			ledOut1=1; ledOut4=1;								// for jumping jacks, red and green LEDs glow
			if(c_JJ>=15)
			{
				ledOut5 = !ledOut5;								//15 reps done, now blue LED blinks
				continue;
			}
			serial.printf("Mode: Jumping Jacks: \t");
			if ((cx >= 20 && cx <= 37)&&(cy >= 0 && cy <= 6)&&(cz>= 0 && cz <= 10))		//active state
			{
				ledOut = 0;
				ledOut5 = 1;					//blue LED glows
				if(g_x > 1.75 )					// counting reps
				{
					if(state_JJ == 0)
					{
						c_JJ++;
					}
					state_JJ = 1;
				}
				else
				state_JJ = 0;
				serial.printf("You are doing it right! Count: %d \n", c_JJ);
			}
			else						//idle state
			{
				ledOut = 1;
				ledOut5 = 0;
				serial.printf("You are doing it wrong! Count: %d \n",c_JJ);
			}
		}
		
		
		
		/* uncomment lines 297 and 299 to note down the classifier values for your orientation if the defined values used 
		in this code don't work */

		/* print angle - avoid doing this in a time sensitive loop */
		//serial.printf("Count: %d :", c_JJ);
		//serial.printf("cx: %d; cy: %d; cz: %d ", cx,cy,cz);

		//serial.printf("ax: %.2f; ay: %.2f; az: %.2f; gx: %.2f; gy: %.2f; gz: %.2f \r\n", a_x,a_y,a_z,g_x,g_y,g_z);
		//serial.printf("gforces: %0.2f \r\n",g_z_filt);
		
		/* sample rate is very approximately 10Hz  - better would be to use the accelerometer to trigger an interrupt*/	
		
		
	}
	
}
