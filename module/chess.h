#ifndef CHESS_H
#define CHESS_H

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/syscalls.h>
#include <linux/ctype.h>
#include <linux/mutex.h>
#include "pieces.h"

#define MODULE_NAME "chess"

#define _MY_PRINTK(once, level, fmt, ...)                       \
    do {                                                          \
        printk##once(KERN_##level "[" MODULE_NAME "]" fmt,    \
                ##__VA_ARGS__);                         \
    } while (0)                                 

#define LOG_INFO(format, ...) _MY_PRINTK(, INFO, format, ##__VA_ARGS__)       

#define LOG_WARN(format, ...) _MY_PRINTK(, WARN, format, ##__VA_ARGS__)       

#define LOG_ERROR(format, ...) _MY_PRINTK(, ERR, format, ##__VA_ARGS__)       

int chess_open(struct inode *pinode, struct file *pfile);
ssize_t chess_read(struct file *pfile, char __user *buffer, size_t length,
                    loff_t *offset);
ssize_t chess_write(struct file *pfile, const char __user *buffer, size_t length,
                    loff_t *offset);
int chess_release(struct inode *pinode, struct file *pfile);

void calcSize(void);
void setupColors(struct piece array[], int color);
void setupBoard(struct piece whitePieces[], struct piece blackPieces[]);
int getIndex(char col, char row);
//void directedMove(int startIndex, int endIndex, char* direction, int* distance);
int kingMove(int startCol, int startRow, int endIndex);
int queenMove(int startCol, int startRow, int endIndex);
int rookMove(int startCol, int startRow, int endIndex);
int bishopMove(int startCol, int startRow, int endIndex);
int knightMove(int startCol, int startRow, int endIndex);
int pawnMove(int startCol, int startRow, int endIndex);
int planeMove(int xPos, int yPos, int endIndex, int xAdd, int yAdd);
int getMyKing(int myPiecesColor);
int amIChecked(int underAttack, struct piece* enemArray);
int possibleMove(char type, int enemCol, int enemRow, int c);

#endif