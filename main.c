#include <stdio.h>
#include <curses.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>     // sleep()
#include <string.h>     // memcpy()

#define ROWS                10
#define COLS                10 * 3

#define CELL(dir, time)     (((dir) << 6) | ((time) & 0x3f))
#define TIME(byte)          ((byte) & 0x3f)
#define DIRECTION(byte)     (((byte) & 0xc0) >> 6)
#define IS_ACTIVE(byte)     TIME(byte)

#define UP                  0x00
#define LEFT                0x01
#define RIGHT               0x02
#define DOWN                0x03
#define NODIR               0xff
#define REVERSE(dir)        (~(dir) & 0x03)

#define MIN(A, B)           ((A) < (B) ? (A) : (B))
#define SUM(A)              (((A) * (A + 1)) >> 1)

#define COORD(y, x)         (((y) << 8) | (x))
#define GET_Y(C)            ((C) >> 8)
#define GET_X(C)            ((C) & 0xff)

#define COLLISION           0x01
#define SECOND              1000000
#define SPEED               8
#define NO_APPLE            0xffff

struct field {
    uint8_t **matrix;
    uint8_t rows;
    uint8_t cols;
};

struct snake {
    uint16_t head;       // H = y      L = x
    uint8_t  length;
    uint8_t  direction;
};

struct snake snake0   = {0, 0, 0};

void die(char* msg)
{
    fputs(msg, stderr);
    fputc('\n', stderr);
    exit(-1);
}

ssize_t clear_screen()
{
    return write(1, "\033[H\033[J", 6);
}

struct field *alloc_field(uint8_t rows, uint8_t cols)
{
    struct field *_f;

/* ---- allocation ---- */    

    _f = calloc(1, sizeof(struct field));
    if (!_f)
        return NULL;

    _f->matrix = calloc(rows, sizeof(uint8_t *));
    if (!_f->matrix)
        goto clean_field;
    _f->rows = 0;

    for (uint8_t i = 0; i < rows; i++) {
        _f->matrix[i] = calloc(cols, 1);
        if (!_f->matrix[i])
            goto clean_matrix;
        _f->rows++;
    }
    _f->cols = cols;

    return _f;

/* ---- cleanup ---- */

clean_matrix:
    for (int i = 0; i < _f->rows; i++)
        free(_f->matrix[i]);
    free(_f->matrix);
clean_field:
    free(_f);

    return NULL;
}

void dealloc_field(struct field *restrict _f)
{
    if (!_f)
        return;

    for (uint8_t i = 0; i < _f->rows; i++)
        free(_f->matrix[i]);
    free(_f->matrix);

    free(_f);
}

void copy_field(struct field *restrict dest, struct field *restrict src)
{
    if (!(dest && src))
        return;

    uint8_t rows = MIN(dest->rows, src->rows);
    uint8_t cols = MIN(dest->cols, src->cols);

    for (uint8_t i = 0; i < rows; i++)
        memcpy(dest->matrix[i], src->matrix[i], cols);
}

struct field *clone_field(struct field *restrict _f)
{
    struct field *_g;

    _g = alloc_field(_f->rows, _f->cols);
    if (!_g)
        return NULL;

    copy_field(_g, _f);

    return _g;
}

char field_ins(struct field *restrict _f, uint8_t y, uint8_t x, uint8_t byte)
{
    if(!_f)
        return -1;

    if (y >= _f->rows || x >= _f->cols)
        return -1;

    _f->matrix[y][x] = byte;
    return 0;
}

char draw_field(struct field *restrict _f)
{
    if (!_f)
        return -1;

    for (uint8_t i = 0; i < _f->rows; i++)
        for (uint8_t j = 0; j < _f->cols; j++)
            mvaddch(i, j, IS_ACTIVE(_f->matrix[i][j]) ? '*' : '.');

    return 0;
}

uint16_t coords(uint16_t limits, uint8_t y, uint8_t x, uint8_t disp)
{
    uint8_t maxy = GET_Y(limits);
    uint8_t maxx = GET_X(limits);

    switch (disp) {
    case UP:
        if (y-- == 0)
            y = maxy - 1;
        break;

    case DOWN:
        if (++y >= maxy)
            y = 0;
        break;

    case LEFT:
        if (x-- == 0)
            x = maxx - 1;
        break;

    case RIGHT:
        if (++x >= maxx)
            x = 0;
        break;
    }

    return COORD(y, x);
}

uint8_t *field_at(struct field *_f, uint16_t coords)
{
    uint8_t x = GET_X(coords);
    uint8_t y = GET_Y(coords);

    if (!_f)
        return NULL;

    return &(_f->matrix[y][x]);
}

uint8_t inc_length(struct field *restrict _f)
{
    for (uint8_t i = 0; i < _f->rows; i++)
        for (uint8_t j = 0; j < _f->cols; j++)
            if (IS_ACTIVE(_f->matrix[i][j]))
                _f->matrix[i][j]++;

    return ++(snake0.length);
}

char update_byte(struct field *_f, uint8_t byte, uint8_t y, uint8_t x)
{
    uint8_t *next;      // address of the next cell
    uint16_t limits;
    uint16_t next_pos;
    uint8_t is_head;

    if (!_f)
        return -1;

    if (!IS_ACTIVE(byte))
        return 0;
    
    is_head = (TIME(byte) == snake0.length);

    limits = COORD(_f->rows, _f->cols);
    next_pos = coords(limits, y, x,
                      is_head ? snake0.direction : DIRECTION(byte));
    next = field_at(_f, next_pos);

    if (is_head) {
        snake0.head = next_pos;
        *next = CELL(snake0.direction, snake0.length);
    }

    _f->matrix[y][x] = byte - 1;    // Update this cell

    return 0;
}

char update_field(struct field *_f)
{
    struct field *tmp = clone_field(_f);

    if (!_f)
        return -1;
    
    for (uint8_t i = 0; i < _f->rows; i++)
        for (uint8_t j = 0; j < _f->cols; j++)
            if (IS_ACTIVE(_f->matrix[i][j]))
                update_byte(tmp, _f->matrix[i][j], i, j);

    copy_field(_f, tmp);
    dealloc_field(tmp);
    return 0;
}

uint16_t checksum(struct field *restrict _f, uint8_t start, uint8_t length)
{
    uint16_t count = 0;
    uint8_t tmp;

    for (uint8_t i = 0; i < _f->rows; i++) {
        for (uint8_t j = 0; j < _f->cols; j++) {
            tmp = TIME(_f->matrix[i][j]);
            if (tmp >= start || tmp <= length)
                count += tmp;
        }
    }

    return count;
}

uint8_t get_direction(char c)
{
    char *letters = "wads";
    uint8_t inverse;

    inverse = REVERSE(snake0.direction);

    for (uint8_t i = 0; i < 4; i++)
        if (letters[i] == c)
            return (i != inverse) ? i : NODIR;

    return NODIR;
}

char init_snake(struct field *restrict _f, uint16_t pos, uint8_t len, uint8_t dir)
{
    uint8_t x, y, byte;

    x = GET_X(pos);
    y = GET_Y(pos);
    byte = CELL(dir, len);

    if (field_ins(_f, y, x, byte) == -1)
        return -1;

    snake0.head = pos;
    snake0.length = len;
    snake0.direction = dir;

    return 0;
}

uint8_t set_direction(uint8_t dir)
{
    if (dir > 3)
        return NODIR;
    
    return snake0.direction = dir;
}

void screen_init()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
}

uint16_t spawn_apple(struct field *restrict _f)
{
    uint8_t y, x;

    do {
        y = rand() % _f->rows;
        x = rand() % _f->cols;
    } while (IS_ACTIVE(_f->matrix[y][x]));

    return COORD(y, x);
}

static inline void suspend()
{
    while (getch() != 'p');
}

int main()
{
    uint16_t turn = 0, apple = NO_APPLE;
    uint8_t impact, settle = 0;
    struct field *grass;
    
    grass = alloc_field(ROWS, COLS);
    srand(time(0));

    if (init_snake(grass, COORD(ROWS - 1, 0), 3, UP) == -1)
        die("Accessing unaccessible memory");

    screen_init();

    for (uint8_t c = 'w'; c != 'q'; c = getch()) {
        set_direction(get_direction(c));
        
        if (c == 'p')
            suspend();

        if (apple == NO_APPLE) {
            apple = spawn_apple(grass);
        } else if (snake0.head == apple) {
            settle = inc_length(grass) < 63;
            apple = NO_APPLE;
        }

        uint16_t csum     = checksum(grass, 1, snake0.length);
        uint16_t expected = SUM(snake0.length) - settle;
        if (settle)
            settle--;
        impact = csum != expected;

        if (++turn > snake0.length && impact)
            break;
        
        draw_field(grass);
        mvaddch(GET_Y(apple), GET_X(apple), 'o');
        refresh();

        update_field(grass);
        usleep(SECOND / SPEED);
    }

    getch();
    endwin();
    clear_screen();
    
    dealloc_field(grass);

    printf("%d points\nturn %d\n", snake0.length, turn);
    return 0;
}
