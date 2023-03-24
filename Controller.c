#define F_CPU 14745600UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "lcd.h" // LCD 사용을 위해 lcd 헤더 파일을 포함
unsigned int ADC_Data = 0;

#define BIT_SET(a,b) ((a) |=  (1<<(b))) // 해당 비트 1로 SET.
#define BIT_CLEAR(a,b) ((a) &=  ~(1<<(b))) // 해당 비트 0으로 CLEAR.
#define BIT_TOGGLE(a,b) ((a) ^=  (1<<(b))) // 해당 비트 반전 시킨다.

// 문자 전송 함수
void putch_USART0(char data){
	while( !(UCSR0A & (1<<UDRE0)));
	UDR0 = data;
}

// 문자열 전송 함수
void sendBuff_USART0(char *str,int length){
	for(int i=0; i<length; i++){
		putch_USART0(*str); // ‘\0’이 나올 때까지 출력한다.
		LCD_Char(*str);
		_delay_ms(100);
		str++;
	}
}

void Init_USART0_IntCon(void){
	// ① RXCIE1=1(수신 인터럽트 허가), RXEN0=1(수신 허가), TXEN0 = 1(송신 허가)
	UCSR0B = (1 <<TXEN0);
	UBRR0H = 0x00; // ② 9600bps 보오레이트 설정(CLK = 14.7456MHz)
	UBRR0L = 0x5f; // ② 9600bps 보오레이트 설정(CLK = 14.7456MHz)
	sei(); // ③ 인터럽트 동작 시작(전체 인터럽트 허가)
}

void ADC_Init(void)
{
	ADMUX = 0x00;
	ADCSRA = 0x00;
	ADMUX |= (1<<REFS0);
	ADCSRA |= (1<<ADEN) | (1<<ADFR) | (3<<ADPS0);   //ADC사용 허가, 프리러닝, 인터럽트 허용, 8분주
	sei(); // 전역 인터럽트 허가.
}

ISR(ADC_vect)
{
	ADC_Data = ADCW; // ADC 값 읽기.
}

unsigned int Read_ADC_Data(unsigned char adc_input)
{
	unsigned int adc_Data = 0; // ADC_Data 변수 = 0
	
	ADMUX &= ~(0x1F);		// ADMUX 값으로 0으로 클리어
	ADMUX |= (adc_input & 0x07); // adc-input 채널로 초기화
	
	ADCSRA |= (1<<ADSC);	// AD 변환 시작
	while(!(ADCSRA & (1<<ADIF))); // AD 변환 종료 대기
	adc_Data = ADCL;	// ADC 변환 값 읽기. 하위 비트 먼저 읽고 상위 비트를 읽는다.
	adc_Data |= ADCH<<8;
	
	return adc_Data;	// ADC 변환값 반환
}

#define REC_BUFF_MAX_LENGTH 100

unsigned char RecBuff[REC_BUFF_MAX_LENGTH];
unsigned char RecBuffindex;

char STR_forLCD[20]; // LCD 출력 문자 저장 위함
volatile char FB = 'S'; // 전,후진 정보. 초기값은 정지
volatile char RL = 'N'; // 좌회전, 우회전 정보. 초기값은 정지
volatile char SW = 'N'; // EEPROM 저장 카운트 출력 명령. 초기값은 NO - 'N'.

// 패킷 전송 함수
void resPacket(char start, char FB, char RL, char SW){
	unsigned char rescBuff[REC_BUFF_MAX_LENGTH];
	unsigned char resBuffLength = 0;
	
	rescBuff[resBuffLength++] = start; // 시작 비트
	rescBuff[resBuffLength++] = FB; // 전, 후진 정보
	rescBuff[resBuffLength++] = RL; // 좌, 우회전 정보
	rescBuff[resBuffLength++] = SW; // EEPROM 저장 정보 출력 명령.
	
	
	sendBuff_USART0(rescBuff,resBuffLength); // 문자열 전송 함수
}

void main(void)
{
	DDRF = 0x00;
	DDRB = 0xff; // PORTB 출력 설정. DC 엔코더 모터 4개 PORTB에 연결한다.
	DDRD = 0x00; // PIND PD0번 스위치 사용을 위해 입력으로 설정.
	
	PORTB = 0xff;
	
	int adc_X, adc_Y = 0; // 조이스틱 X축 값, Y축 값
	char Message[40];
	
	ADC_Init();
	Init_USART0_IntCon();
	LCD_Init();
	ADCSRA |= (1<<ADSC);

	int flag = 0;

	while (1)
	{
		// EEPROM 저장 정보 출력 명령 선택을 위한 코드
		// PIND에 PD0번 스위치를 눌렀다 떌때마다 flag 값을 반전 시킨다.
		if((PIND & 0x01) == 0x00){
			while((PIND & 0x01) != 1 ){;}
			BIT_TOGGLE(flag , 0);
		}
		
		// flag 값에 따라 SW에 'E' : 차량에 EEPROM 저장된 정보를 출력하라는 명령.
		//					  'N' : 기본 값.
		if((flag & 1) != 0){
			SW = 'E';
		}
		else if((flag & 1) == 0){
			SW = 'N';
		}

		adc_X = Read_ADC_Data(0); // 조이스틱의 X축 값을 읽어온다.
		adc_Y = Read_ADC_Data(1); // 조이스틱의 Y축 값을 읽어온다.
		
		// 조이스틱 Y축 값의 범위에 따라 전진, 후진, 정지 정보를 설정한다.
		if(adc_Y >= 700){FB = 'F';}
		if(adc_Y <= 350){FB = 'B';}
		if((adc_Y < 700) && (adc_Y > 350)){FB = 'S';}
		
		// 조이스틱 X축 값의 범위에 따라 좌회전, 우회전, 정지 정보를 설정한다.
		if(adc_X >= 800){RL = 'R';}
		if(adc_X <= 300){RL = 'L';}
		if((adc_X <= 700) && (adc_X > 300)){RL = 'N';}
		
		// PD0번 스위치와 조이스틱에 의해 정해진 명령 정보를 차량부에 패킷으로 전송한다.
		resPacket('A', FB, RL, SW);
		
		// 조이스틱 X값을 확인하기 위해 LCD에 출력한다.
		sprintf(Message,"X: %04d ", adc_X);
		LCD_Pos(0,0);
		LCD_Str(Message);
		
		// 조이스틱 Y값을 확인하기 위해 LCD에 출력한다.
		sprintf(Message,"Y: %04d ", adc_Y);
		LCD_Pos(1,0);
		LCD_Str(Message);
	}
}


