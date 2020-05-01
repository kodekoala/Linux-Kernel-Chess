#ifndef PIECES_H
#define PIECES_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/ctype.h>
#include <linux/mutex.h>

#define KING 'K'
#define QUEEN 'Q'
#define BISHOP 'B'
#define KNIGHT 'N'
#define ROOK 'R'
#define PAWN 'P'

#define WHITE 'W'
#define BLACK 'B'

struct piece
{
    //members
    char color;  
    char type;
    int moved;
    int index;
    int alive;
    // int col;
    // int row;
};

static struct piece* board[8*8] = {NULL};
static struct piece whitePieces[16];
static struct piece blackPieces[16];



#endif