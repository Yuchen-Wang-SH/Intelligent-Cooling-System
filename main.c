/*
@author
Yuchen Wang

@Processor
STC89C52: 8-bit Microcontroller
	ROM: 8K bytes flash
	RAM: 256 bytes, so we need two hex numbers for addressing.

@Chip
HD7279A: used to connect to LED and keyboard.

@Temperature_Sensor
DS18B20

@Memory
24C16: A 16K E2PROM (Electrically Erasable Programmable Read-Only Memory)
	It can store data even without electricity, which is different from RAM.

@Other
Cooling Fan
Power
*/

#include <REG52.H>	/* Special function register declarations */             
#include "Somenop.h"

#include "HD7279A.h"
#include "DS18B20.h"
#include "EEPROM_24C16.h"
#include "UART.h"
#include "main.h"

#include <stdio.h>                
#include <math.h>
#include <string.h>

unsigned char LEDValue[50] = {
	0xFC,0x44,0x79,0x5D,0xC5,0x9D,0xBD,0x54,0xFD,0xDD,0xF5,0xAD,0xB8,0x6D,0xB9,0xB1, //0-F
	0xFE,0x46,0x7B,0x5F,0xC7,0x9F,0xBF,0x56,0xFF,0xDF,0xF7,0xAF,0xBA,0x6F,0xBB,0xB3, //0.-F.
	0x00,0xA9,0xF1,0x21,0x2C,0x25,0x2D,0x01 // 32null，33t，34P,35r,36u,37n,38o,39-
}; 

/* The memory map of LEDs */
unsigned char DispBuff[8] = {32,32,32,32,32,32,32,32};

unsigned char KeyTable[4] = {0x3B, 0x3A, 0x39, 0x38}; // down up back enter

unsigned char run_options[10];
// The start address in C16 where you store the configurations of motor tests.
unsigned char run_options_start_address = 0;

unsigned char temp_threshold[2];
unsigned char temp_threshold_start_address = 10;

unsigned char temperature[2];

unsigned char motor_thre;
unsigned char motor_now = 0;

unsigned char timerH = 0xFF;
unsigned char timerL = 0x00;

unsigned char counter = 0;

unsigned char temp[5]; // bai shi ge shifen baifen

unsigned char PWM = 50;

unsigned char TEMP_CONTROL_MODE = FALSE;

unsigned char KeyFlag = 0;

// Only changing PID configuration needs password. You can set the pwd here.
unsigned char PIDPwd[4] = {0,0,0,0};
unsigned char PIDGoalTempAddress = 12;

struct _pid {
	unsigned char goal_temp[3]; // Temperature: xx.x
	unsigned char current_temp[3];
	unsigned char Kp, Ki, Kd;
	float current_PWM;
	float err, err_last;
	float integral;
} pid;

void main (void) {
  	Init_7279();
  	DS18B20_Init();
	Motor = 0;
  	display_string_in_row("tP- ", TRUE);
	TMOD = 0x01;
	TH0 = timerH;
	TL0 = timerL;
	EA = 1;
	ET0 = 1;
 	show_main_menu();
}

/*	Look up function of a character.
	@param A character to display.
	@return The index of this character in the LEDValue array.
*/
unsigned char check_LED (char c) {
	if (c >= '0' && c <= '9') return (c - '0');
	else if (c >= 'A' && c <= 'F') return (c - 'A' + 10);
	switch (c) {
		case ' ': return 32;
		case 't': return 33;
		case 'P': return 34;
		case 'r': return 35;
		case 'u': return 36;
		case 'n': return 37;
		case 'o': return 38;
		case '-': return 39;
		case 'b': return 11;
		case 'd': return 13;
		default: return -1;
	}
}

void show_main_menu(void) {
	/* All the functions we provide */
	char* menu_buffer[] = {"tP- ", "run-", "Con-", "PA- ", "P1d-"};
	int num_functions = 5;
	/* The index of the current selected menu in the menu_buffer */
	unsigned char current_menu = 0;
	while(1) {
		display(DispBuff);
		Key();
		if (KeyValue != 0xff) {   
		// 有键按下，按照键号执行菜单显示或进入子菜单
			if (KeyNum == DOWN) {
				current_menu = change_menu_ptr(current_menu, FALSE, num_functions);
				display_string_in_row(menu_buffer[current_menu], TRUE);
			} else if (KeyNum == UP) {
				current_menu = change_menu_ptr(current_menu, TRUE, num_functions);
				display_string_in_row(menu_buffer[current_menu], TRUE);
			} else if (KeyNum == ENTER) {
				switch(current_menu) {
					case 0: refresh_show_temperature(FALSE); break;
					case 1: show_motor_test(); break;
					case 2:	con_with_temp(FALSE); 
							display_string_in_row("Con-", TRUE); 
							Motor = 0; 
							break;
					case 3: show_PA_menu(); break;
					case 4: con_with_temp(TRUE); 
							display_string_in_row("Con-", TRUE); 
							Motor = 0;
							break;
				}
				/* Clear the LEDs. */
				display_string_in_row("    ", FALSE);
			}
		}
  }
}

/*	Refresh the LEDs to show the digits in buff. */
void display(unsigned char buff[]) {
	unsigned char i;
	for (i = 0; i < 8; i++) {
		write_7279(0x90+i, LEDValue[buff[i]]);	 
	}
}

void Key(void) {
   unsigned char temp,i;
   temp = ReadKey();  //读键值
   if (temp==0xff) {
	KeyNum = 0xff;	
	KeyValue = 0xff;
	KeyFlag = 0;
   } else { 
	    // After button released, it should not be considered as still having a key pressed.
		if (KeyValue!=0xff && KeyFlag < 250) {
			KeyNum=0xff;
			KeyFlag++;
		} else {
			KeyValue = temp;  
			for(i=0;i<=3;i++)
				if (KeyValue == KeyTable[i])
				{ KeyNum = i; break; }	
		}

   }	
}

/*	Display the string on the LED. Since LED has two rows of four digits, 
	if upper is TRUE, the string will be displayed on the top layer, otherwise
	lower layer.
*/
void display_string_in_row(char* string, unsigned char upper) {
	unsigned char start_index;
	unsigned char i;
	if (upper == TRUE) start_index = 4;
	else start_index = 0;
	
	for (i = 0; i < 4; i++) {
		DispBuff[start_index+i] = check_LED(string[i]);
	}
	display(DispBuff);
}

void display_int_in_row(unsigned char i, unsigned char upper) {
	unsigned char quotient, rem;
	unsigned char current_index;
	if (upper == TRUE)	current_index = 7;
	else current_index = 3;
	display_string_in_row("    ", upper);
	if (i == 0) {
		DispBuff[current_index] = 0;
		display(DispBuff);
		return;
	}
	while (TRUE) {
		rem = i % 10;
		quotient = i / 10;
		if (rem == 0 && quotient == 0) break;
		DispBuff[current_index] = rem;	//print out remainder from right to left
		current_index--;
		i /= 10;
	}
	display(DispBuff);
}

/*	Scroll between different menu options.
	@param
	current: index of the current selected function.
	inc: whether it is move to next one or last one.
	volume: total number of available functions.
	@return: the next selected index
*/
unsigned char change_menu_ptr(unsigned char current, unsigned char inc, unsigned char volume) {
	if (inc == TRUE) {
		if (current == volume - 1) return 0;
		else return ++current;
	}
	else {
		if (current == 0) return (volume - 1);
		else return --current;
	}
}

void show_PA_menu(void) {
	char* menu_buffer[] = {" run", " Con", " P1d"};
	unsigned char current_menu = 0;
	display_string_in_row(" run ", FALSE);
	while(1) {
		display(DispBuff); //显示（按显缓单元的内容显示）
		Key();
		if(KeyValue != 0xff) { 
			if (KeyNum == DOWN) {
				current_menu = change_menu_ptr(current_menu, FALSE, 3);
				display_string_in_row(menu_buffer[current_menu], FALSE);
			}
			else if (KeyNum == UP) {
				current_menu = change_menu_ptr(current_menu, TRUE, 3);
				display_string_in_row(menu_buffer[current_menu], FALSE);
			}
			else if (KeyNum == ENTER) {
				switch(current_menu) {
					case 0: show_current_run();
							display_string_in_row("PA- ", TRUE);
							display_string_in_row(" run", FALSE);
							break;
					case 1: show_current_temp_threshold(); 
							display_string_in_row("PA- ", TRUE);
							display_string_in_row(" Con", FALSE);
							break;
					case 2: show_current_PID_goal_temp();
							display_string_in_row("PA- ", TRUE);
							display_string_in_row(" P1d", FALSE);
							break;
				}
			}
			else if (KeyNum == BACK) {
				return;
			}
		}
	}
}

void show_current_run(void) {
	unsigned char current_menu = 0;

	read_run_options_from_C16();
	display_string_in_row("P- 0", TRUE);
	display_int_in_row(run_options[0], FALSE);

	while (1) {
		// display(DispBuff); //显示（按显缓单元的内容显示）
		Key();
		if (KeyValue != 0xff) { 
			if (KeyNum == DOWN) {
				current_menu = change_menu_ptr(current_menu, FALSE, 10);
				DispBuff[7] = current_menu;
				display_int_in_row(run_options[current_menu], FALSE);
				// display_string_in_row(menu_buffer[current_menu], FALSE);
			}
			else if (KeyNum == UP) {
				current_menu = change_menu_ptr(current_menu, TRUE, 10);
				DispBuff[7] = current_menu;
				display_int_in_row(run_options[current_menu], FALSE);
				// display_string_in_row(menu_buffer[current_menu], FALSE);
			}
			else if (KeyNum == ENTER) {
				DispBuff[4] = 10;
				// run_options[current_menu] = change_num_conti(run_options[current_menu]);
				change_num_conti(&run_options[current_menu]);
				write_run_options_to_C16();
				wait_until_release();
				DispBuff[4] = 34; // "P"
				display(DispBuff);
			}
			if (KeyNum == BACK){
				write_run_options_to_C16();
				return;
			}
		}
	}
}

void show_current_temp_threshold(void) {
	unsigned char current_menu = 0;
	char* menu_buffer[2] = {"PA-b", "PA-F"};

	read_temp_threshold_from_C16();
	display_string_in_row(menu_buffer[current_menu], TRUE);
	display_int_in_row(temp_threshold[current_menu], FALSE);

	while (1) {
		Key();
		if (KeyValue != 0xff) { 
			if (KeyNum == DOWN) {
				current_menu = change_menu_ptr(current_menu, FALSE, 2);
				display_string_in_row(menu_buffer[current_menu], TRUE);
				display_int_in_row(temp_threshold[current_menu], FALSE);
			}
			else if (KeyNum == UP) {
				current_menu = change_menu_ptr(current_menu, TRUE, 2);
				display_string_in_row(menu_buffer[current_menu], TRUE);
				display_int_in_row(temp_threshold[current_menu], FALSE);
			}
			
			else if (KeyNum == ENTER) {
				DispBuff[4] = 10;
				DispBuff[5] = 39;	// "-"
				DispBuff[6] = 32;	// " "
				// temp_threshold[current_menu] = change_num_conti(temp_threshold[current_menu]);
				change_num_conti(&temp_threshold[current_menu]);
				write_temp_threshold_to_C16();
				DispBuff[4] = 34; // "P"
				DispBuff[5] = 10;
				DispBuff[6] = 39;
				display(DispBuff);
			}
			if (KeyNum == BACK){
				write_temp_threshold_to_C16();
				return;
			}
		}
	}
}


void change_num_conti(unsigned char *num) {
	unsigned char original = *num;

	display(DispBuff);
	wait_until_release();
	while (TRUE) {
		Key();
		switch (KeyNum) {
			case UP: (*num) = change_menu_ptr(*num, TRUE, 100); 
					display_int_in_row(*num, FALSE);
					break;
			case DOWN: (*num) = change_menu_ptr(*num, FALSE, 100); 
					display_int_in_row(*num, FALSE);
					break;
			case BACK: (*num) = original;
					display_int_in_row(*num, FALSE);
					return;
			case ENTER: return;
			default: break;
		}
	}
}

void wait_until_release(void) {
	while (KeyNum != 0xff || KeyValue != 0xff) {
		Key();
	}
	return;
}



void refresh_show_temperature(unsigned char upper) {
	unsigned char i, count;
	count = 0;
	InitUART();
	while(1) {
		Key();
		if (KeyNum == BACK) return;
		DS18B20_Reset();
		DS18B20_WriteData(0xcc);
		DS18B20_WriteData(0x44);

		DS18B20_Reset();
		DS18B20_WriteData(0xcc);
		DS18B20_WriteData(0xbe);

		for (i = 0; i < 2; i++) {
			temperature[i] = DS18B20_ReadData();
		}
		DS18B20_Reset();

		display_temperature(upper);
		count++;
		if (count == 100) {
			send_temp_to_computer();
			count = 0;
		}
		Somenop50();
		if(TEMP_CONTROL_MODE) break;
	}
}

void display_temperature(unsigned char upper) {
	unsigned char start_address = 0;
	U8 temp_data,temp_data_2 ;
	U16 TempDec	;

	if (upper) start_address = 4;
	temp_data = temperature[1];
	temp_data &= 0xf0; //取高4 位
	if (temp_data == 0xf0) { //判断是正温度还是负温度读数
		DispBuff[start_address + 0] = 39;//负温度读数求补,取反加1,判断低8 位是否有进位
		if (temperature[0] == 0){
	 //有进位,高8 位取反加1
		temperature[0] = ~temperature[0] + 1;
		temperature[1] = ~temperature[1] + 1;
		} else {
		 //没进位,高8 位不加1
		temperature[0] = ~temperature[0] + 1;
		temperature[1] = ~temperature[1];
		}
	}

	temp_data = (temperature[1] & 0x07) << 4;  //取高字节低4位(温度读数高4位)，注意此时是12位精度
	temp_data_2 = temperature[0] >> 4; //取低字节高4位(温度读数低4位)，注意此时是12位精度
	temp_data = temp_data | temp_data_2; //组合成完整数据
	temp[0] = temp_data / 100;  //取百位转换为ASCII码
	temp[1] = (temp_data % 100) / 10; //取十位转换为ASCII码
	temp[2] = (temp_data % 100) % 10; //取个位转 换为ASCII码
	temperature[0] &= 0x0f;  //取小数位转换为ASCII码
	TempDec = temperature[0] * 625;  //625=0.0625* 10000,  表示小数部分，扩大1万倍，方便显示
	temp[3] = TempDec / 1000;  //取小数十分位转换为ASCII码
	temp[4] = (TempDec % 1000) / 100;  //取小数百分位转换为ASCII码
	if(DispBuff[start_address + 0] == 39){
		if(temp[0] != 0) {
			DispBuff[start_address+1] = temp[0];
			DispBuff[start_address+2] = temp[1];
			DispBuff[start_address+3] = temp[2];
		} else {
			DispBuff[start_address+1] = temp[1];
			DispBuff[start_address+2] = temp[2] + 16;
			DispBuff[start_address+3] = temp[3];
		}
	} else {
		if(temp[0] != 0) {
			DispBuff[start_address+0] = temp[0];
			DispBuff[start_address+1] = temp[1];
			DispBuff[start_address+2] = temp[2] + 16;
			DispBuff[start_address+3] = temp[3];
		} else {
			DispBuff[start_address+0] = temp[1];
			DispBuff[start_address+1] = temp[2] + 16;
			DispBuff[start_address+2] = temp[3];
			DispBuff[start_address+3] = temp[4];
		}
	}
	display(DispBuff);
}

void show_motor_test(void) {
	unsigned char current_menu = 0;
	unsigned char num_configs = 10;
	TEMP_CONTROL_MODE = FALSE;
	read_run_options_from_C16();
	display_string_in_row("r- 0", TRUE);
	display_int_in_row(run_options[0], FALSE);

	while (1) {
		// display(DispBuff); //显示（按显缓单元的内容显示）
		Key();
		if (KeyValue != 0xff) { 
			if (KeyNum == DOWN) {
				current_menu = change_menu_ptr(current_menu, FALSE, num_configs);
				DispBuff[7] = current_menu;
				display_int_in_row(run_options[current_menu], FALSE);
			} else if (KeyNum == UP) {
				current_menu = change_menu_ptr(current_menu, TRUE, num_configs);
				DispBuff[7] = current_menu;
				display_int_in_row(run_options[current_menu], FALSE);
			} else if (KeyNum == ENTER) {
				display_string_in_row("run-", TRUE);
				PWM = run_options[current_menu];
				run_motor_with_PWM();
				DispBuff[4] = 35;	// r
				DispBuff[5] = 39;	// -
				DispBuff[6] = 32;	// space
				DispBuff[7] = current_menu;
				display_int_in_row(run_options[current_menu], FALSE);
				wait_until_release();
			} else if (KeyNum == BACK) {
				DispBuff[4] = 35;	// r
				DispBuff[5] = 36;	// -
				DispBuff[6] = 37;	// space
				DispBuff[7] = 39;
				return;
			}
		}
	}
}


void run_motor_with_PWM() {
	unsigned char i;

	motor_thre = PWM;

	while(1) {
		if (TEMP_CONTROL_MODE) {
			for (i = 0; i < 10; i++) {
				TR0 = 1;
				while (TR0 == 1);
			}
			break;
		} else {
			TR0 = 1;
			Key();
			if (KeyNum == BACK) {
				TR0 = 0;
				Motor = 0;
				return;
			}
		}
	}

}

void timer0(void) interrupt 1 using 3 {
	TH0 = timerH;
	TL0 = timerL;

	if (motor_now < motor_thre) {
		Motor = 1;
		motor_now++;
		P2 = 0x00;
	}
	else if (motor_now < 100) {
		Motor = 0;
		motor_now++;
		P2 = 0x02;
	}
	else {
		motor_now = 0;
		if (TEMP_CONTROL_MODE)	TR0 = 0;
	}	
}

void con_with_temp(unsigned char usingPID) {
	unsigned int i = 0;
	TEMP_CONTROL_MODE = TRUE;
	PIDInit();
	while(1) {
		Key();
		if (KeyNum == BACK){
			TEMP_CONTROL_MODE = FALSE;
			return;
		}
		refresh_show_temperature(TRUE);
		if (i % 6 == 0)	display_int_in_row(PWM, FALSE);
		Key();
		if (KeyNum == BACK) {
			TEMP_CONTROL_MODE = FALSE;
			return;
		}
		if (usingPID)	calc_PWM_PID();
		else	calc_current_PWM();
		Key();
		if (KeyNum == BACK){
			TEMP_CONTROL_MODE = FALSE;
			return;
		}
		run_motor_with_PWM();
		Key();
		if (KeyNum == BACK){
			TEMP_CONTROL_MODE = FALSE;
			return;
		}
		i++;

	}
}

void calc_current_PWM(){
	// Now we have temp[]. We have to change PWM.
	unsigned char current_temp = 10 * temp[1] + temp[2];
	unsigned int nominator, denom;

	read_temp_threshold_from_C16();
	nominator = (temp[1] * 1000 + temp[2] * 100 + temp[3] * 10 + temp[4]) - temp_threshold[0]* 100;
	denom = 2 * (temp_threshold[1] - temp_threshold[0]);
	if (current_temp >= temp_threshold[1]) {
		PWM = 100; return;
	}
	else if (current_temp < temp_threshold[0]) {
		PWM = 0; 
		return;
	}
	PWM = nominator / denom + 50;
}

void read_run_options_from_C16() {
	unsigned char i;
	for(i = 0; i < 10; i++) {
		run_options[i] = eread_add(run_options_start_address+i);
	}
}

void write_run_options_to_C16() {
	unsigned char i;
	for(i = 0; i < 10; i++) {
		ewrite_add(run_options_start_address+i, run_options[i]);
	}
}

void read_temp_threshold_from_C16() {
	unsigned char i;
	for(i = 0; i < 2; i++) {
		temp_threshold[i] = eread_add(temp_threshold_start_address+i);
	}
}

void write_temp_threshold_to_C16() {
	unsigned char i;
	for(i = 0; i < 2; i++) {
		ewrite_add(temp_threshold_start_address+i, temp_threshold[i]);
	}
}

void show_current_PID_goal_temp() {
	unsigned char i;
	unsigned char flag = check_pwd();
	if (flag == FALSE)	return;
	display_string_in_row("A- P", TRUE);
	read_PID_goal_temp_from_C16();
	DispBuff[0] = 32;
	DispBuff[1] = pid.goal_temp[0];
	DispBuff[2] = pid.goal_temp[1];
	DispBuff[3] = pid.goal_temp[2];
	display(DispBuff);

	flag = FALSE;
	for(i = 1; i < 4; i++){
		while(TRUE) {
			Key();
			switch (KeyNum) {
				case BACK: return;
				case ENTER: flag = TRUE; 
							pid.goal_temp[i-1] = DispBuff[i];
							break;
				case UP:
					if (i != 2) {
						DispBuff[i] = change_menu_ptr(DispBuff[i], TRUE, 10); 
					}
					else {
						DispBuff[i] = change_menu_ptr(DispBuff[i]-16, TRUE, 10);
						DispBuff[i] += 16;
					}
					display(DispBuff);
					break;
				case DOWN:
					if (i != 2){
						DispBuff[i] = change_menu_ptr(DispBuff[i], FALSE, 10); 
					} else {
						DispBuff[i] = change_menu_ptr(DispBuff[i]-16, FALSE, 10);
						DispBuff[i] += 16;
					}
					display(DispBuff);
					break;
			}
			if (flag) {
				flag = FALSE;
				break;
			}
		}
		write_PID_goal_temp_to_C16();
	}
}

void read_PID_goal_temp_from_C16() {
	unsigned char i;
	for(i = 0; i < 3; i++) {
		pid.goal_temp[i] = eread_add(PIDGoalTempAddress+i);
		if (pid.goal_temp[i] >= 10 && i != 1)	pid.goal_temp[i] = 0;
	}
}

void write_PID_goal_temp_to_C16() {
	unsigned char i;
	for(i = 0; i < 3; i++) {
		ewrite_add(PIDGoalTempAddress+i, pid.goal_temp[i]);
	}
}

unsigned char check_pwd() {
	unsigned char i;
	unsigned char flag = FALSE;
	display_string_in_row("PA55", TRUE); // Look like PASS in LED.
	display_string_in_row("0000", FALSE);
	
	for(i = 0; i < 4; i++) {
		while(TRUE) {
			Key();
			switch (KeyNum) {
				case BACK: return FALSE;
				case UP: DispBuff[i] = change_menu_ptr(DispBuff[i], TRUE, 10);
						 	display(DispBuff); 
						 	break;
				case DOWN: DispBuff[i] = change_menu_ptr(DispBuff[i], FALSE, 10); 
							display(DispBuff); 
							break;
				case ENTER: flag=TRUE; 
							break;
			}
			if (flag) {
				flag = FALSE;
				break;
			}
		}
	}
	for(i = 0; i < 4; i++) {
		if (DispBuff[i] != PIDPwd[i])	return FALSE;
	}
	return TRUE;
}

void PIDInit() {
	pid.err_last = 0;
	pid.integral = 0;
}

void calc_PWM_PID() {
	unsigned char i;
	float current_tempFloat, goalTempFloat;
	read_PID_goal_temp_from_C16();
	// PIDInit();
	goalTempFloat = pid.goal_temp[0] * 10 + (pid.goal_temp[1]-16) + pid.goal_temp[2] * 0.1;
	pid.Kp = 50;
	pid.Ki = 0;
	pid.Kd = 20;

	for (i = 0; i < 3; i++) {
		pid.current_temp[i] = temp[i+1];
	}
	current_tempFloat = pid.current_temp[0] * 10 + pid.current_temp[1] + pid.current_temp[2] * 0.1;

	pid.err = current_tempFloat - goalTempFloat;

	if (pid.err < 0) {
		PWM = 0;
		return;
	}
	pid.integral += pid.err;
	pid.current_PWM = pid.Kp * pid.err + pid.Ki * pid.integral + pid.Kd * (pid.err - pid.err_last) + 50;
	pid.err_last = pid.err;
	if (pid.current_PWM >= 100)	pid.current_PWM = 100;
	else if (pid.current_PWM <= 0) pid.current_PWM = 50;

	PWM = pid.current_PWM;
}

void send_temp_to_computer(){
	SendOneByte(temp[1] * 10 + temp[2]);
	SendOneByte(temp[3] * 10 + temp[4]);
}