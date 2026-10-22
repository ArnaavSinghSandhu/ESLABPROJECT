#include <lpc17xx.h>
#include <stdio.h>
#include <string.h>

/* LCD connections */
#define LCD_DATA_MASK  (0x0FU << 23)
#define RS_CTRL        (1U << 27)
#define EN_CTRL        (1U << 28)

/* Keypad connections */
#define ROW_MASK       (0x0FU << 10)
#define COL_MASK       (0x0FU << 23)

/* LEDs */
#define LED_GREEN      (1U << 18)
#define LED_RED        (1U << 19)
#define LED_YELLOW     (1U << 20)

/* Seven-segment display */
#define MSSD_DATA_MASK (0xFFU << 4)
#define MSSD_SEL_MASK  (0x0FU << 15)

/* LCD variables used by lab-manual style functions */
unsigned long int temp1, temp2;
unsigned char flag1, flag2;

/* Quiz structure */
typedef struct
{
    char question[32];
    char opt1[16];
    char opt2[16];
    char opt3[16];
    char opt4[16];
    char correct_ans;
} Question;

Question quiz[] =
{
    {"Which is LPC1768 CPU?", "1. Cortex-M0", "2. Cortex-M3",
     "3. Cortex-M4", "4. ARM7", '2'},

    {"LPC1768 Flash size?", "1. 256KB", "2. 128KB",
     "3. 512KB", "4. 64KB", '3'},

    {"Default Timer tick?", "1. 1 ms", "2. 1 sec",
     "3. 1 us", "4. 10 ms", '1'}
};

#define TOTAL_QUESTIONS (sizeof(quiz) / sizeof(quiz[0]))

/* Function prototypes */
void System_Init(void);
void Delay_ms(unsigned int ms);

void delay_lcd(unsigned int count);
void port_write(void);
void lcd_write(void);
void LCD_Command(unsigned char cmd);
void LCD_Data(unsigned char data);
void LCD_Init(void);
void LCD_String(char *str);
void LCD_SetCursor(unsigned char row, unsigned char col);
void LCD_Clear(void);

char Keypad_Scan(void);

void SevenSeg_Display(int num);
void Timer0_Init(void);
char Play_Countdown(int seconds);

/* Delay */
void Delay_ms(unsigned int ms)
{
    unsigned int i, j;

    for (i = 0; i < ms; i++)
        for (j = 0; j < 2000; j++);
}

/* LCD lab-manual delay */
void delay_lcd(unsigned int count)
{
    volatile unsigned int i;

    for (i = 0; i < count; i++);
}

/*
 * Writes the selected LCD nibble.
 * temp2 contains the nibble already shifted to P0.23-P0.26.
 */
void port_write(void)
{
    LPC_GPIO0->FIOCLR = LCD_DATA_MASK;
    LPC_GPIO0->FIOSET = temp2;

    if (flag1 == 0)
        LPC_GPIO0->FIOCLR = RS_CTRL;
    else
        LPC_GPIO0->FIOSET = RS_CTRL;

    LPC_GPIO0->FIOSET = EN_CTRL;
    delay_lcd(25);
    LPC_GPIO0->FIOCLR = EN_CTRL;
    delay_lcd(30000);
}

/* LCD byte write, adapted from the lab manual */
void lcd_write(void)
{
    flag2 = (flag1 == 1) ? 0 :
            (((temp1 == 0x30) || (temp1 == 0x20)) ? 1 : 0);

    temp2 = temp1 & 0xF0;
    temp2 = temp2 << 19;
    port_write();

    if (flag2 == 0)
    {
        temp2 = temp1 & 0x0F;
        temp2 = temp2 << 23;
        port_write();
    }
}

void LCD_Command(unsigned char cmd)
{
    flag1 = 0;
    temp1 = cmd;
    lcd_write();
}

void LCD_Data(unsigned char data)
{
    flag1 = 1;
    temp1 = data;
    lcd_write();
}

/* LCD initialization using the lab-manual command sequence */
void LCD_Init(void)
{
    LPC_GPIO0->FIODIR |= LCD_DATA_MASK | RS_CTRL | EN_CTRL;

    LPC_GPIO0->FIOCLR = LCD_DATA_MASK | RS_CTRL | EN_CTRL;

    Delay_ms(20);

    LCD_Command(0x30);
    Delay_ms(5);

    LCD_Command(0x30);
    Delay_ms(1);

    LCD_Command(0x30);
    Delay_ms(1);

    LCD_Command(0x20);  /* Enter 4-bit mode */
    LCD_Command(0x28);  /* 2 lines, 5x7 font */
    LCD_Command(0x0C);  /* Display ON, cursor OFF */
    LCD_Command(0x06);  /* Cursor increment */
    LCD_Command(0x01);  /* Clear display */
    LCD_Command(0x80);  /* First line, first position */

    Delay_ms(5);
}

void LCD_String(char *str)
{
    while (*str)
        LCD_Data((unsigned char)*str++);
}

void LCD_SetCursor(unsigned char row, unsigned char col)
{
    if (row == 0)
        LCD_Command(0x80 + col);
    else
        LCD_Command(0xC0 + col);
}

void LCD_Clear(void)
{
    LCD_Command(0x01);
    Delay_ms(2);
}

/* Keypad scanning: assumes active-HIGH column signals */
char Keypad_Scan(void)
{
    const char lookup[4][4] =
    {
        {'1', '2', '3', 'A'},
        {'4', '5', '6', 'B'},
        {'7', '8', '9', 'C'},
        {'*', '0', '#', 'D'}
    };

    unsigned int col_state;
    int r, c;

    for (r = 0; r < 4; r++)
    {
        LPC_GPIO2->FIOCLR = ROW_MASK;
        LPC_GPIO2->FIOSET = (1U << (10 + r));

        Delay_ms(1);

        col_state = (LPC_GPIO1->FIOPIN & COL_MASK) >> 23;

        if (col_state != 0)
        {
            for (c = 0; c < 4; c++)
            {
                if (col_state & (1U << c))
                {
                    char key = lookup[r][c];

                    while (((LPC_GPIO1->FIOPIN & COL_MASK)
                            >> 23) != 0)
                    {
                        Delay_ms(1);
                    }

                    Delay_ms(20);
                    return key;
                }
            }
        }
    }

    return '\0';
}

/* Seven-segment codes: common-cathode convention */
unsigned char seg_code[10] =
{
    0x3F, 0x06, 0x5B, 0x4F, 0x66,
    0x6D, 0x7D, 0x07, 0x7F, 0x6F
};

void SevenSeg_Display(int num)
{
    int units, tens;

    if (num < 0 || num > 99)
        return;

    units = num % 10;
    tens = num / 10;

    LPC_GPIO0->FIOCLR = MSSD_SEL_MASK | MSSD_DATA_MASK;
    LPC_GPIO0->FIOSET = (1U << 16);
    LPC_GPIO0->FIOSET = ((unsigned int)seg_code[tens] << 4);
    Delay_ms(5);

    LPC_GPIO0->FIOCLR = MSSD_SEL_MASK | MSSD_DATA_MASK;
    LPC_GPIO0->FIOSET = (1U << 15);
    LPC_GPIO0->FIOSET = ((unsigned int)seg_code[units] << 4);
    Delay_ms(5);
}

/* Timer0 configuration: assumes PCLK = 25 MHz */
void Timer0_Init(void)
{
    LPC_SC->PCONP |= (1U << 1);

    /* PCLK = CCLK / 4 */
    LPC_SC->PCLKSEL0 &= ~(3U << 2);

    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->PR = 25000U - 1U;
    LPC_TIM0->TC = 0;
    LPC_TIM0->PC = 0;
    LPC_TIM0->TCR = 0x01;
}

/* Countdown returns the selected answer or '\0' on timeout */
char Play_Countdown(int seconds)
{
    unsigned int elapsed;
    int remaining;
    char key;

    LPC_TIM0->TCR = 0x02;
    LPC_TIM0->TC = 0;
    LPC_TIM0->PC = 0;
    LPC_TIM0->TCR = 0x01;

    while (1)
    {
        elapsed = LPC_TIM0->TC;

        if (elapsed >= (unsigned int)seconds * 1000U)
            break;

        remaining = seconds - (int)(elapsed / 1000U);

        SevenSeg_Display(remaining);

        key = Keypad_Scan();

        if (key >= '1' && key <= '4')
            return key;
    }

    return '\0';
}

/* System initialization */
void System_Init(void)
{
    /* Ensure relevant pins operate as GPIO */
    LPC_PINCON->PINSEL1 &= ~(
        (3U << 14) | (3U << 16) | (3U << 18) |
        (3U << 20) | (3U << 22) | (3U << 24) |
        (3U << 26) | (3U << 28) | (3U << 30) |
        (3U << 0)  | (3U << 2)  | (3U << 4)
    );

    LPC_PINCON->PINSEL0 &= ~(
        (3U << 8)  | (3U << 10) | (3U << 12) |
        (3U << 14) | (3U << 16) | (3U << 18) |
        (3U << 20) | (3U << 22)
    );

    LPC_GPIO0->FIODIR |= MSSD_DATA_MASK | MSSD_SEL_MASK;

    LPC_GPIO1->FIODIR &= ~COL_MASK;
    LPC_GPIO2->FIODIR |= ROW_MASK;

    LPC_GPIO1->FIODIR |= LED_GREEN | LED_RED | LED_YELLOW;
    LPC_GPIO1->FIOCLR = LED_GREEN | LED_RED | LED_YELLOW;
}

int main(void)
{
    int score, q_idx;
    char user_input;
    char score_msg[32];

    System_Init();
    LCD_Init();
    Timer0_Init();

    while (1)
    {
        score = 0;

        LCD_Clear();
        LCD_SetCursor(0, 0);
        LCD_String("LPC1768 QUIZ");
        LCD_SetCursor(1, 0);
        LCD_String("Press 1 to Start");

        while (Keypad_Scan() != '1');

        for (q_idx = 0; q_idx < TOTAL_QUESTIONS; q_idx++)
        {
            LCD_Clear();
            LCD_SetCursor(0, 0);
            LCD_String("Question ");
            LCD_Data('1' + q_idx);

            LCD_SetCursor(1, 0);
            LCD_String(quiz[q_idx].question);
            Delay_ms(20000);

            LCD_Clear();
            LCD_SetCursor(0, 0);
            LCD_String(quiz[q_idx].opt1);
            LCD_SetCursor(1, 0);
            LCD_String(quiz[q_idx].opt2);
            Delay_ms(20000);

            LCD_Clear();
            LCD_SetCursor(0, 0);
            LCD_String(quiz[q_idx].opt3);
            LCD_SetCursor(1, 0);
            LCD_String(quiz[q_idx].opt4);
            Delay_ms(20000);

            LCD_Clear();
            LCD_SetCursor(0, 0);
            LCD_String("Answer: 1-4");
            LCD_SetCursor(1, 0);
            LCD_String("Time: 10 sec");

            user_input = Play_Countdown(10);

            LPC_GPIO1->FIOCLR =
                LED_GREEN | LED_RED | LED_YELLOW;

            LCD_Clear();

            if (user_input == '\0')
            {
                LPC_GPIO1->FIOSET = LED_YELLOW;
                LCD_String("Time Expired!");
            }
            else if (user_input == quiz[q_idx].correct_ans)
            {
                LPC_GPIO1->FIOSET = LED_GREEN;
                score++;
                LCD_String("Correct Answer!");
            }
            else
            {
                LPC_GPIO1->FIOSET = LED_RED;
                LCD_String("Wrong Answer!");
            }

            Delay_ms(1500);
        }

        LCD_Clear();
        sprintf(score_msg, "Score: %d / %d",
                score, (int)TOTAL_QUESTIONS);
        LCD_SetCursor(0, 0);
        LCD_String(score_msg);

        LCD_SetCursor(1, 0);
        LCD_String("Press # Restart");

        while (Keypad_Scan() != '#');
    }
}