#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "efm32gg.h"
#include "audio.h"

int8_t keys_typed = 0x00;

double walltime(void);
void start_walltime(void);
void stop_walltime(void);
void clear_walltime(void);

void play_square(int wave_frequency);
void play_sawtooth(int wave_frequency);
void set_current_song(int n);

void update_key_controller(void);
void update_song_controller(void);
void update_volume_controller(void);

#define BASE_FREQUENCY (14000000)

#define SAMPLE_FREQUENCY (44100)

/* Extracts bit 'n' from 's' */
#define BIT(s, n) (((s) >> (n)) & 1U)

enum { SW1, SW2, SW3, SW4, SW5, SW6, SW7 };

enum { SONG1, SONG2, SONG3 };

int8_t volume = 4;

typedef struct Song Song;

struct Song {
    int *frequency;
    int *duration;
    int length;
} current_song;

int song1_frequency[] = {
    100, 200, 300, 400, 500, 600, 700, 800, 900
};
int song1_duration[] = {
    100, 100, 100, 100, 100, 100, 100, 100, 100
};
int song1_length = 9;

int song2_frequency[] = {
    900, 800, 700, 600, 500, 400, 300, 200, 100
};
int song2_duration[] = {
    100, 100, 100, 100, 100, 100, 100, 100, 100
};
int song2_length = 9;

int song3_frequency[] = {
    1000, 1200
};
int song3_duration[] = {
    100, 200
};
int song3_length = 2;

int main(void)
{
    /* Enable GPIO */
    *CMU_HFPERCLKEN0 |= CMU2_HFPERCLKEN0_GPIO;

    /* Configure gamepad */
    *GPIO_PC_MODEL = 0x33333333;
    *GPIO_PC_DOUT = 0xFF;

    /* Configure LEDs */
    *GPIO_PA_CTRL = 2;
    *GPIO_PA_MODEH = 0x55555555;
    *GPIO_PA_DOUT = 0x00FF;

    /* Configure DAC */
    *CMU_HFPERCLKEN0 |= CMU2_HFPERCLKEN0_DAC0;
    *DAC0_CTRL = 0x50010;
    *DAC0_CH0CTRL = 1;
    *DAC0_CH1CTRL = 1;

    /* Configure TIMER1 */
    *CMU_HFPERCLKEN0 |= (1 << 6);
    *TIMER1_CMD = 1;

    /* Configure TIMER2 */
    *CMU_HFPERCLKEN0 |= (1 << 7);
    *TIMER2_TOP = ((1 << 16) - 1);
    *TIMER2_CTRL |= 0xa000000;
    *TIMER2_CMD = 1;

    /* Configure interrupt handling for GPIO_ODD and GPIO_EVEN */
    *ISER0 |= 0x802;

    /* Configure interrupt generation for gamepad */
    *GPIO_IEN = 0xFF;
    *GPIO_EXTIPSELL = 0x22222222;
    *GPIO_EXTIRISE = 0xFF;
    *GPIO_EXTIFALL = 0xFF;

    while (1)
    {
        if (*TIMER1_CNT > BASE_FREQUENCY / SAMPLE_FREQUENCY)
        {
            *TIMER1_CNT = 0;
            update_song_controller();
        }
    }

	return 1;
}


/*------------------------------------------------------------------------------
 *
 * Walltime functions
 *
 *----------------------------------------------------------------------------*/


/* Clears and starts the walltime */
void start_walltime(void)
{
    *TIMER2_CNT = 0;
    *TIMER2_CMD = 1;
}

/* Clears the walltime */
void clear_walltime(void)
{
    *TIMER2_CNT = 0;
}

/* Stops the walltime */
void stop_walltime(void)
{
    *TIMER2_CMD = 2;
}

/* Returns the walltime in milliseconds

   The walltime is calculated using TIMER2 with a pre-scale factor of 10 using
   BASE_FREQUENCY. Thus the theoretical time limit using this function is

       (1000 * (2^16 - 1)) / (BASE_FREQUENCY / 2^10)

   milliseconds.
*/
double walltime(void)
{
    return (double)(*TIMER2_CNT * 1000 / (BASE_FREQUENCY / (int)(1 << 10)));
}

void play_square(int wave_frequency)
{
    static int x = 0;
    const int T = SAMPLE_FREQUENCY / wave_frequency;
    const int H = T / 2;

    int delta = 3;

    if (x < H)
        delta = 0;

    x = (x + 1) % T;

    *DAC0_CH0DATA = (int16_t)(volume * delta);
    *DAC0_CH1DATA = (int16_t)(volume * delta);
}

void play_sawtooth(int wave_frequency)
{
    static int x = 0;
    const int T = SAMPLE_FREQUENCY / wave_frequency;
    const int H = T / 4;

    int delta = 3;

    if (x < H)
        delta = 0;
    else if (x < 2 * H)
        delta = 1;
    else if (x < 3 * H)
        delta = 2;

    x = (x + 1) % T;

    *DAC0_CH0DATA = (int16_t)(volume * delta);
    *DAC0_CH1DATA = (int16_t)(volume * delta);
}


/*------------------------------------------------------------------------------
 *
 * Interrupt controllers
 *
 *----------------------------------------------------------------------------*/


void __attribute__ ((interrupt)) GPIO_EVEN_IRQHandler()
{
    update_key_controller();
    update_volume_controller();
    *GPIO_IFC = 0xFF;
}

void __attribute__ ((interrupt)) GPIO_ODD_IRQHandler()
{
    update_key_controller();
    update_volume_controller();
    *GPIO_IFC = 0xFF;
}


/*------------------------------------------------------------------------------
 *
 * Key controller
 *
 *----------------------------------------------------------------------------*/


int8_t keys_curr_state = 0xff;
int8_t keys_prev_state = 0xff;

void update_key_controller(void)
{
    keys_prev_state = keys_curr_state;
    keys_curr_state = ~(*GPIO_PC_DIN);

    keys_typed = 0x00;

    /* Song controls: SW1, SW2, SW3 */
    if (BIT(keys_curr_state, SW1) && !BIT(keys_prev_state, SW1))
        keys_typed = (1 << SW1);
    else if (BIT(keys_curr_state, SW2) && !BIT(keys_prev_state, SW2))
        keys_typed = (1 << SW2);
    else if (BIT(keys_curr_state, SW3) && !BIT(keys_prev_state, SW3))
        keys_typed = (1 << SW3);

    /* Volume controls: SW5, SW7 */
    else if (BIT(keys_curr_state, SW7) && !BIT(keys_prev_state, SW7))
        keys_typed = (1 << SW7);
    else if (BIT(keys_curr_state, SW5) && !BIT(keys_prev_state, SW5))
        keys_typed = (1 << SW5);
}


/*------------------------------------------------------------------------------
 *
 * Audio controller
 *
 *----------------------------------------------------------------------------*/


void set_current_song(int n)
{
    switch (n)
    {
    case SONG1:
        current_song.frequency = song1_frequency;
        current_song.duration = song1_duration;
        current_song.length = song1_length;
        break;
    case SONG2:
        current_song.frequency = song2_frequency;
        current_song.duration = song2_duration;
        current_song.length = song2_length;
        break;
    case SONG3:
        current_song.frequency = song3_frequency;
        current_song.duration = song3_duration;
        current_song.length = song3_length;
        break;
    }
}

void update_song_controller(void)
{
    enum { PLAY, IDLE };

    static int i = 0;
    static int state = IDLE;

    if (BIT(keys_typed, SW1))
    {
        i = 0;
        set_current_song(SONG1);
        state = PLAY;
        start_walltime();
    }
    else if (BIT(keys_typed, SW2))
    {
        i = 0;
        set_current_song(SONG2);
        state = PLAY;
        start_walltime();
    }
    else if (BIT(keys_typed, SW3))
    {
        i = 0;
        set_current_song(SONG3);
        state = PLAY;
        start_walltime();
    }

    keys_typed = 0x00;

    switch (state)
    {
    case IDLE:
        break;
    case PLAY:
        if (walltime() < current_song.duration[i])
        {
            play_sawtooth(current_song.frequency[i]);
        }
        else
        {
            clear_walltime();
            i++;
            if (i > current_song.length - 1)
            {
                i = 0;
                state = IDLE;
                stop_walltime();
            }
        }
        break;
    }
}


/*------------------------------------------------------------------------------
 *
 * Volume controller
 *
 *----------------------------------------------------------------------------*/


void update_volume_controller(void)
{
    int8_t volume_bar[] = {
        0xff, /* Min */
        0xfe,
        0xfc,
        0xf8,
        0xf0,
        0xe0,
        0xc0,
        0x80,
        0x00  /* Max */
    };

    if (BIT(keys_typed, SW5))
    {
        volume--;
        if (volume < 0)
            volume = 0;
    }
    if (BIT(keys_typed, SW7))
    {
        volume++;
        if (volume > 8)
            volume = 8;
    }

    *GPIO_PA_DOUT = (volume_bar[volume] << 8) | 0xff;
}
