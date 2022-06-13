#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define A PD5
#define B PD1
#define C PD2
#define D PD0
#define E PD7
#define F PD4
#define G PD3
#define DP PB0

// Common for digits is PC1:4
void init_display(){
    DDRC &= ~((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));
    PORTC &= ~((1 << 1) | (1 << 2) | (1 << 3) | (1 << 4));

    // Set all segments as outputs
    DDRD |= (1 << A) | (1 << B) | (1 << C) | (1 << D)
            | (1 << E) | (1 << F) | (1 << G);
            
    // Decimal Point
    DDRB |= (1 << DP);

    // Setup timer to automatically update display
    TCCR0 = (1 << CS00);
    TIMSK |= (1 << TOIE0);
}

void draw_number(char num){
    int result = 0;

    switch(num){
        case 0:
            result = ~(1 << G);
            break;

        case 1:
            result = (1 << B) | (1 << C);
            break;

        case 2:
            result = ~((1 << F) | (1 << C));
            break;

        case 3:
            result = ~((1 << F) | (1 << E));
            break;

        case 4:
            result = ~((1 << A) | (1 << E) | (1 << D));
            break;

        case 5:
            result = ~((1 << B) | (1 << E));
            break;
    
        case 6:
            result = ~(1 << B);
            break;

        case 7:
            result = (1 << A) | (1 << B) | (1 << C);
            break;

        case 8:
            result = 0xff;
            break;

        case 9:
            result = ~(1 << E); 
            break;

        case 'p':
            result = ~((1 << C) | (1 << D));
            break;

        case 'n':
            result = (1 << E) | (1 << G) | (1 << C);
            break;

        case 'u':
            result = (1 << E) | (1 << D) | (1 << C);
            break;

        case 'a':
            result = ~(1 << D);
            break;

        case 'b':
            result = ~((1 << A) | (1 << B));
            break;

        case 'c':
            result = (1 << G) | (1 << E) | (1 << D);
            break;

        case 'd':
            result = ~((1 << A) | (1 << F));
            break;

        case 'e':
            result = ~((1 << B) | (1 << C));
            break;

        case 'f':
            result = ~((1 << B) | (1 << C) | (1 << D));
            break;

        case '-':
        default:
            result = (1 << G);
            break;
    }

    PORTD = result;
}

void clear_display(){
    PORTD = 0;
    PORTB &= ~(1 << DP);
}

unsigned char display[] = {0, 1, 2, 'u', 2};
int digit = 0;
void update_display(){
    clear_display();
    DDRC = 0;

    // Select digit
    DDRC |= (1 << digit+1);
    draw_number(display[digit]);

    // Decimal point
    if(display[4] == digit)
        PORTB |= (1 << DP);

    digit++;

    if(digit > 3){
        digit = 0;
    }
}

void set_dp(char loc){
    display[4] = loc;
}

ISR (TIMER0_OVF_vect) {
    update_display();
}


void init_comparator(){
    // Disable ADC
    ADCSRA &= ~(1 << ADEN);

    // Enable ADC0 to be used as AIN1
    SFIOR |= (1 << ACME);
    ADMUX &= ~((1 << MUX0) | (1 << MUX1) | (1 << MUX2) | (1 << MUX3));

    // Input capture on timer1
    ACSR = (1 << ACIC);

    // Setup Timer1
    // Enable noise canceler, no prescaler, and trigger on rising edge
    TCCR1B = (1 << ICNC1) | (1 << CS10) | (1 << ICES1);
}

void disable_comp(){
    // Disable input capture interrupt
    TIMSK &= ~(1 << TICIE1);
}

void enable_comp(){
    // Enable input capture interrupt
    TIMSK |= (1 << TICIE1);
}

void init_cap_meter(){
    // All inputs initally
    DDRB &= ~(0b11111 << 1);
    PORTB = 0;
}

void discharge_cap(){
    DDRB |= (1 << PB5);
    _delay_ms(1000);
    DDRB &= ~(1 << PB5);
}

char current_res = 3;

void move_res_up(){
    if(current_res <= 1)
        return;

    current_res--;
}

void move_res_down(){
    if(current_res >= 5)
        return;

    current_res++;
}

char success = 0;
ISR(TIMER1_CAPT_vect){
    unsigned int val = ICR1;
    // Usually garbage above a certain point
    if(val > 50000 || val < 10)
        return;

    char unit = 'u';
    switch(current_res){
        case 5: // 10+ uF
            val /= 10000;
            set_dp(-1);
            unit = 'u';
            break;

        case 4: // 0.1-10uF
            val /= 100;
            set_dp(1);
            unit = 'u';
            break;

        case 3: // 10nF-100nF
            val /= 10;
            set_dp(-1);
            unit = 'n';
            break;

        case 2: // 1nF-10nF
            set_dp(1);
            unit = 'n';
            break;

        case 1: // pF
            val *= 10;
            set_dp(1);
            unit = 'p';
            break;
    }

    display[3] = unit;
    display[2] = val%10;
    display[1] = (val/10)%10;
    display[0] = (val/100)%10;

    // Too fast
    if(val < 10){
        move_res_up();
    } else if (val > 1000) { // Too slow
        move_res_down();
    }
}

void charge_cap(){
    char resistor = 4;
    if(current_res == 5)
        resistor = 5

    DDRB |= (1 << resistor);
    enable_comp();
    PORTB |= (1 << resistor);
    TCNT1 = 0; // Reset timing
    _delay_ms(250);
    disable_comp();
    PORTB &= ~(1 << resistor);
    DDRB &= ~(1 << resistor);
}

int main(void){
    init_display();
    init_comparator();
    init_cap_meter();

    sei();

    while(1){
        discharge_cap();
        charge_cap();
    }
}
