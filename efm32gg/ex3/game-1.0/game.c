#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 1
#define _GNU_SOURCE 1
#endif /* _POSIX_C_SOURCE */

#include <inttypes.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

/* Extracts bit 'n' of 's' */
#define BIT(s, n) (((s) >> (n)) & 1U)

FILE *gamepad = NULL;

/* Screen dimensions */
#define WIDTH 320
#define HEIGHT 240
int window_w = WIDTH;
int window_h = HEIGHT;

typedef uint16_t pixel_t;

pixel_t *screen;
struct fb_copyarea screen_region;
int screen_fd;

/* Key mapping according to bit possition */
enum { SW1, SW2, SW3, SW4, SW5, SW6, SW7, SW8 };

/* Snake movement directions */
enum { UP, DOWN, RIGHT, LEFT };

int current_direction = RIGHT;
int next_direction = RIGHT;


/*------------------------------------------------------------------------------
 *
 * Utils
 *
 *----------------------------------------------------------------------------*/


void print_bits(int value)
{
    int i = 0;
    while (i < 8)
        printf("%d", (int)((value >> i++) & 0x01));
    printf("\n");
}

double walltime(void)
{
    static struct timeval t;
    gettimeofday(&t, NULL);
    return (double)t.tv_sec + (double)t.tv_usec * 1.0e-6;
}


/*------------------------------------------------------------------------------
 *
 * SIGIO handler
 *
 *----------------------------------------------------------------------------*/


void interrupt_handler(int signo)
{
    int8_t key_pressed = 0x00;

    read(fileno(gamepad), &key_pressed, 1);

    key_pressed = ~key_pressed;

    if (BIT(key_pressed, SW1))
        next_direction = LEFT;
    if (BIT(key_pressed, SW3))
        next_direction = RIGHT;
    if (BIT(key_pressed, SW2))
        next_direction = UP;
    if (BIT(key_pressed, SW4))
        next_direction = DOWN;

    /* NOP when direction is opposite to current direction */
    switch (next_direction)
    {
    case UP:
        if (current_direction == DOWN)
            return;
        break;
    case DOWN:
        if (current_direction == UP)
            return;
        break;
    case RIGHT:
        if (current_direction == LEFT)
            return;
        break;
    case LEFT:
        if (current_direction == RIGHT)
            return;
        break;
    }

    current_direction = next_direction;
}


/*------------------------------------------------------------------------------
 *
 * Driver setup
 *
 *----------------------------------------------------------------------------*/


int init_gamepad()
{
    gamepad = fopen("/dev/gamepad", "rb+");

    int fd = fileno(gamepad);

    if (!gamepad)
    {
        printf("Failed opening gamedpad driver\n");
        return 1;
    }

    /* Set interrupt handler for SIGIO */
    if (signal(SIGIO, &interrupt_handler) == SIG_ERR)
    {
        printf("Failed binding function\n");
        return 1;
    }
    /* Set ownership of file descriptor to this process */
    if (fcntl(fd, F_SETOWN, getpid()) == -1)
    {
        printf("Failed setting owner\n");
        return 1;
    }
    /* Register this process to recieve SIGIO from driver */
    if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | FASYNC) == -1)
    {
        printf("Failed setting FASYNC flag\n");
        return 1;
    }

    return 0;
}

int init_graphics()
{
    screen_fd = open("/dev/fb0", O_RDWR);

    if (screen_fd == -1)
    {
        printf("Failed to open display driver\n");
        return 1;
    }

    screen_region.dx = 0;
    screen_region.dy = 0;
    screen_region.width = window_w;
    screen_region.height = window_h;

    screen = (uint16_t*)mmap(NULL, window_w * window_h * sizeof(pixel_t),
        PROT_READ | PROT_WRITE, MAP_SHARED, screen_fd, 0);

    if (screen == MAP_FAILED)
    {
        printf("Failed to map pixel array to framebuffer\n");
        return 1;
    }

    return 0;
}


/*------------------------------------------------------------------------------
 *
 * Snake
 *
 *----------------------------------------------------------------------------*/


#define COLOR_BACKGROUND 0xffff
#define COLOR_APPLE 0xf9a6
#define COLOR_SNAKE 0x4208

#define SQUARE_SIZE (int)(10)
#define N_SQUARES (int)(WIDTH * HEIGHT / (SQUARE_SIZE * SQUARE_SIZE))
#define N_SQUARES_X (int)(WIDTH / SQUARE_SIZE)
#define N_SQUARES_Y (int)(HEIGHT / SQUARE_SIZE)

typedef struct Snake Snake;
typedef struct Square Square;

struct Square
{
    int x;
    int y;
} apple;

struct Snake
{
    int x[N_SQUARES];
    int y[N_SQUARES];
    int length;
} snake;

int apple_collision(void);
int snake_collision(void);
void draw_square(int x, int y, int size, pixel_t color);
void init_snake(int x, int y, int length);
void print_board_info(void);
void init_game(void);
void update_direction(void);
void update_snake(void);

void init_snake(int x, int y, int length)
{
    current_direction = RIGHT;
    next_direction    = RIGHT;

    for (int i = 0; i < length; i++)
    {
        snake.x[i] = x - i * SQUARE_SIZE;
        snake.y[i] = y;
    }
    snake.length = length;
}

void update_snake(void)
{
    int *x = snake.x;
    int *y = snake.y;

    int x_prev[snake.length];
    int y_prev[snake.length];

    memcpy(x_prev, x, snake.length * sizeof(int));
    memcpy(y_prev, y, snake.length * sizeof(int));

    for (int i = 1; i <= snake.length; i++)
    {
        x[i] = x_prev[i - 1];
        y[i] = y_prev[i - 1];
    }

    switch (current_direction)
    {
    case UP:
        y[0] -= SQUARE_SIZE;
        break;
    case DOWN:
        y[0] += SQUARE_SIZE;
        break;
    case RIGHT:
        x[0] += SQUARE_SIZE;
        break;
    case LEFT:
        x[0] -= SQUARE_SIZE;
        break;
    }

    for (int i = 0; i < snake.length; i++)
    {
        if (x[i] < 0)
            x[i] = WIDTH - SQUARE_SIZE;
        else if (x[i] >= WIDTH)
            x[i] = 0;
        else if (y[i] < 0)
            y[i] = HEIGHT - SQUARE_SIZE;
        else if (y[i] >= HEIGHT)
            y[i] = 0;
    }
}

void draw_square(int x, int y, int size, pixel_t color)
{
    for (int i = x; i < x + size; i++)
        for (int j = y; j < y + size; j++)
            screen[window_w * j + i] = color;
}

int snake_collision(void)
{
    for (int i = 3; i < snake.length; i++)
        if ((snake.x[0] == snake.x[i]) && (snake.y[0] == snake.y[i]))
            return 1;
    return 0;
}

int apple_collision(void)
{
    return (apple.x == snake.x[0]) && (apple.y == snake.y[0]);
}

void init_game(void)
{
    /* Init snake */
    init_snake(WIDTH / 2, HEIGHT / 2, 3);
    for (int i = 0; i < snake.length; i++)
        draw_square(snake.x[i], snake.y[i], SQUARE_SIZE, COLOR_SNAKE);

    /* Init background */
    memset(screen, COLOR_BACKGROUND, window_w * window_h * sizeof(pixel_t));

    /* Init apple */
    apple.x = (rand() % N_SQUARES_X) * SQUARE_SIZE;
    apple.y = (rand() % N_SQUARES_Y) * SQUARE_SIZE;
    draw_square(apple.x, apple.y, SQUARE_SIZE, COLOR_APPLE);
}

int main(int argc, char *argv[])
{
    init_graphics();
    init_gamepad();

    printf("Running game...\n");

    init_game();

    while (1)
    {
        /* Draw snake */
        draw_square(snake.x[snake.length - 1], snake.y[snake.length - 1],
            SQUARE_SIZE, COLOR_BACKGROUND);
        update_snake();
        draw_square(snake.x[0], snake.y[0], SQUARE_SIZE, COLOR_SNAKE);

        if (apple_collision())
        {
            /* Extend snake */
            snake.x[snake.length] = snake.x[snake.length - 1];
            snake.y[snake.length] = snake.y[snake.length - 1];
            snake.length++;

            /* Seed apple */
            apple.x = (rand() % N_SQUARES_X) * SQUARE_SIZE;
            apple.y = (rand() % N_SQUARES_Y) * SQUARE_SIZE;
        }
        else if (snake_collision())
        {
            init_game();
        }

        /* Draw apple */
        draw_square(apple.x, apple.y, SQUARE_SIZE, COLOR_APPLE);

        ioctl(screen_fd, 0x4680, &screen_region);
    }

    fclose(gamepad);
    munmap(screen, window_w * window_h * sizeof(pixel_t));
    close(screen_fd);

    return 0;
}
