#include <stdio.h>
#include <stdint.h>
#include <malloc.h>

typedef struct
{
    uint8_t pos_x;
    uint8_t pos_y;
} Player;

typedef struct
{
    Player player;
} Game;

int main(void)
{
    Game* game = malloc(sizeof(Game));
    *game = (Game){0};

    printf("Hello, World!\n");
    printf("Player_Pos = %d", game->player.pos_y);

    free(game);
    return 0;
}