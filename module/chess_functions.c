#include "chess.h"
#include "pieces.h"

char unknownCmd[] = {'U', 'N', 'K', 'C', 'M', 'D', '\n'};
char invalidFmt[] = {'I', 'N', 'V', 'F', 'M', 'T', '\n'};
char charBoard[129] = { '*' };
char mate[] = {'M', 'A', 'T', 'E', '\n'};
char check[] = {'C', 'H', 'E', 'C', 'K', '\n'};
char illMove[] = {'I', 'L', 'L', 'M', 'O', 'V', 'E', '\n'};
char oot[] = {'O', 'O', 'T', '\n'};
char noGame[] = {'N', 'O', 'G', 'A', 'M', 'E', '\n'};
char ok[] = {'O', 'K', '\n'};

char* returnStr = NULL;

char wb[] = {'W', 'B'};
int turn = 0;
char* player1 = NULL; 
char* cpu = NULL; 
int gameOver = 1;
int buffer_size = 0;

/* Declare mutex */
DEFINE_MUTEX(lock);

int chess_open(struct inode *pinode, struct file *pfile){
    LOG_INFO("In function %s\n", __FUNCTION__);

    charBoard[128] = '\n';

    //In open we return 0 for a successful open, otherwise we should return an error
    return 0;
}

ssize_t chess_read(struct file *pfile, char __user *buffer, size_t length,
                    loff_t *offset){
    int rv = 0;
    /* Lock */
    mutex_lock(&lock);
    
    LOG_INFO("In function %s\n", __FUNCTION__);
    LOG_INFO("In function %s the length is %zd\n", __FUNCTION__, length);
    
    if (buffer_size){
        if (copy_to_user(&buffer[0], &returnStr[0], buffer_size) != 0){
            LOG_INFO("Copy/Allocation Error\n");
            mutex_unlock(&lock);
            return -EFAULT;
        }
        rv = buffer_size;
        length = buffer_size;
    }
    buffer_size = 0;

    /* Unlock */
    mutex_unlock(&lock);

    //We return how many bytes someone reads from our file when they call read on it
    return rv;
}

ssize_t chess_write(struct file *pfile, const char __user *buffer, size_t length,
                    loff_t *offset){
    
    char * kernelBuff;
    long colVal, rowVal;
    char * st;
    char *endptr;
    char ** tokenArr;
    char *dest;
    int intStartCol, intStartRow, checkCol, checkRow;
    int tempRow, tempCol, enemRow, enemCol;
    int myKingLoc, enemKingLoc, numCheckers;
    struct piece* tempPiece;
    struct piece* myArray;
    struct piece* enemArray; 
    char color, piece, startC, startR, destC, destR;
    char enemColor = '\0', enemPiece = '\0', transColor = '\0', transPiece = '\0';
    int startIndex = 0, endIndex = 0, numTokens = 0, illegal = 1, possible = 0, notMate = 0;
    int tempIndex;
    int i = 0, c = 0;
    long temp = 0;
    int MAXLEN = 17;
    
    /*Example: WPf7-g8xBNyWB\n  ... this has a length of 14 */
    

    
    /*The format is: ColorPieceColRow-DestColDestRowxCappedColorPieceyPromoColorPiece */
    /*Example: WPf7-g8xBNyWB\n  ... this has a length of 14 */
    /*Therefore the longest valid command: "02 MOVE" will be 17 chars long*/

    /* Lock */
    mutex_lock(&lock);
    LOG_INFO("In function %s\n", __FUNCTION__);
    LOG_INFO("In function %s the length is %zd\n", __FUNCTION__, length);

    /*
    Check that *buffer is a valid pointer, and also that you can read the length provided from it
    */

    if (buffer == NULL || length < 0){ //check passed in pointer
        LOG_ERROR("Invalid buffer or length passed\n");
        mutex_unlock(&lock);
        return -EFAULT;
    }

    if (!access_ok(buffer, length)){ 
        LOG_ERROR("Buffer inaccessible or not as large as specified\n");
        mutex_unlock(&lock);
        return -EFAULT;
    }

    if (length == 0){
        LOG_INFO("Empty command!");
        returnStr = unknownCmd;
        calcSize();
        return length;
    }

    /*
    If the command length is more than the maximum length for an accepted one...
    */
    if (length > MAXLEN){
        LOG_INFO("Length greater than 17\n");

        kernelBuff = kmalloc (2, GFP_KERNEL);

        if(copy_from_user( &kernelBuff[0], &buffer[0], sizeof(kernelBuff)) != 0){
            LOG_INFO("Copy/Allocation Error\n");
            kfree(kernelBuff);
            mutex_unlock(&lock);
            return -EFAULT;
        }

        returnStr = unknownCmd;
        if (kernelBuff[0] == '0'){
            if (kernelBuff[1] == '0' || kernelBuff[1] == '1' || kernelBuff[1] == '2' ||
             kernelBuff[1] == '3' || kernelBuff[1] == '4'){
                returnStr = invalidFmt;
            }
        }
        calcSize();
        kfree(kernelBuff);
        mutex_unlock(&lock);
        return length;
    }

    kernelBuff = kmalloc (length, GFP_KERNEL);

    if(!kernelBuff){
        LOG_INFO("Allocation Error\n");
        mutex_unlock(&lock);
        return -EFAULT;
    }

    if(copy_from_user( &kernelBuff[0], &buffer[0], length) != 0){
        LOG_INFO("Copy/Allocation Error\n");
        kfree(kernelBuff);
        mutex_unlock(&lock);
        return -EFAULT;
    }

    if (*kernelBuff == 0 || kernelBuff[0] == '\n'){
        LOG_INFO("Empty command!");
        returnStr = unknownCmd;
        calcSize();
        kfree(kernelBuff);
        mutex_unlock(&lock);
        return length;
    }

    if (kernelBuff[length - 1] != '\n'){
        LOG_INFO("Invalid command, no newline char!");
        returnStr = unknownCmd;
        calcSize();
        kfree(kernelBuff);
        mutex_unlock(&lock);
        return length;
    }

    tokenArr = kmalloc(10 * sizeof(char*), GFP_KERNEL);

    if (tokenArr == NULL){
        LOG_INFO("Allocation Error\n");
        kfree(kernelBuff);
        mutex_unlock(&lock);
        return -ENOMEM;
    }

    for (i = 0; i < 10; i++){
        tokenArr[i] = NULL;    
    }

    kernelBuff[strcspn(kernelBuff, "\n")] = '\0';
    st = kernelBuff;

    i = -1;
    while((dest = strsep(&st, " ")) != NULL){
        i++;
        numTokens = i + 1;

        /*Give extra byte for null terminator \0 */
        tokenArr[i] = kmalloc (strlen(dest) + 1, GFP_KERNEL);
        if (tokenArr[i] == NULL){
            LOG_INFO("Allocation Error\n");
            for (c = 0; c < i; c++){
                kfree(tokenArr[c]);
            }
            kfree(tokenArr);
            kfree(kernelBuff);
            mutex_unlock(&lock);
            return -ENOMEM;
        }
        memcpy(tokenArr[i], dest, strlen(dest) + 1);
    }

    /*Options for valid commands:
    Begin a new game: 00 W/B
    Curr state of board: 01
    02 MOVE
    CPU make move: 03
    Resign, CPU wins: 04
    */

    LOG_INFO("Number of tokens is: %d\n", numTokens);

    if (numTokens == 1){
        if (strcmp(tokenArr[0], "01") == 0){
            /*The command is 01*/
            goto case1;
        }
        else if (strcmp(tokenArr[0], "03") == 0){
            /*The command is 03*/
            goto case3;
        }
        else if (strcmp(tokenArr[0], "04") == 0){
            /*The command is 04*/
            goto case4;
        }
    }
    else if (numTokens == 2){
        if (strcmp(tokenArr[0], "00") == 0){
            illegal = 0;
            if(strcmp(tokenArr[1], "W") == 0 || strcmp(tokenArr[1], "B") == 0){
                LOG_INFO("The command is 00\n");
                /*The command is 00*/
                player1 = &wb[1];
                cpu = &wb[0];
                if (strcmp(tokenArr[1], "W") == 0){
                    player1 = &wb[0];
                    cpu = &wb[1];
                }
                goto case0;
            }
        }
        else if (strcmp(tokenArr[0], "02") == 0){
            goto case2;
        }
    }
    
    if (strcmp(tokenArr[0], "00") == 0 || strcmp(tokenArr[0], "01") == 0 || strcmp(tokenArr[0], "02") == 0 
        || strcmp(tokenArr[0], "03") == 0 || strcmp(tokenArr[0], "04") == 0){
            illegal = 0;
        }

    returnStr = invalidFmt;
    if (illegal){
        returnStr = unknownCmd; 
    }
    goto clearMem;

    case0:
        for (i = 0; i < 128; i++){
            charBoard[i] = '*';
        }

        setupColors(whitePieces, 0);
        setupColors(blackPieces, 1);

        setupBoard(whitePieces, blackPieces);

        returnStr = ok;
        gameOver = 0;
        turn = 0;
        goto clearMem;

    case1:
        returnStr = charBoard;
        if (gameOver){
            returnStr = noGame;
        }
        goto clearMem;

    case2:
        /*Example: WPf7-g8xBNyWB\n  ... this has a length of 14 */
        if (gameOver){
            returnStr = noGame;
            goto clearMem;
        }

        if (*player1 != wb[turn]){
            LOG_ERROR("Turn isn't right!\n");
            returnStr = oot;
            goto clearMem;
        }
        
        
        temp = strlen(tokenArr[1]);

        LOG_INFO("The len of the second token is: %ld", temp);

        if (temp == 7){
            goto len7;
        }
        else if (temp == 10){
            goto len10;
        }
        else if (temp == 13){
            goto len13;
        }
        else{
            LOG_INFO("Entered move is illegal, length of second token is wrong\n");
            returnStr = invalidFmt;
            goto clearMem;
        }

        /*'K' for king, 'Q' for queen, 'B' for bishop, 'N' for knight, 'R' for rook, and 'P' for pawn*/
        /*color, piece, startC, startR, destC, destR, enemColor, enemPiece, transColor, transPiece */
    len13:
        if (tokenArr[1][temp - 4] == 'y'){
            if (tokenArr[1][temp - 3] == 'W' || tokenArr[1][temp - 3] == 'B'){
                if (tokenArr[1][temp - 3] == tokenArr[1][0]){
                    if (tokenArr[1][temp - 2] == 'Q' || tokenArr[1][temp - 2] == 'B' 
                        || tokenArr[1][temp - 2] == 'N' || tokenArr[1][temp - 2] == 'R'){
                            if (tokenArr[1][1] == 'P'){
                                transColor = tokenArr[1][temp - 3];
                                transPiece = tokenArr[1][temp - 2];
                                goto len10;
                            }
                    }
                }
            }
        }
        /*Clear memory and return!*/
        LOG_INFO("Entered move is illegal\n");
        returnStr = illMove;
        goto clearMem;

    len10:
        if (tokenArr[1][7] == 'x'){
            if (tokenArr[1][8] == 'W' || tokenArr[1][8] == 'B'){
                if (tokenArr[1][8] != tokenArr[1][0]){
                    if (tokenArr[1][9] == 'Q' || tokenArr[1][9] == 'B' 
                        || tokenArr[1][9] == 'N' || tokenArr[1][9] == 'R'
                        || tokenArr[1][9] == 'P'){
                            enemColor = tokenArr[1][8];
                            enemPiece = tokenArr[1][9];
                            goto len7;
                    }
                }
            }
        }
        /*Clear memory and return!*/
        LOG_INFO("Entered move is illegal\n");
        returnStr = illMove;
        goto clearMem;
        
        /*Example: WPf7-g8xBNyWB\n  ... this has a length of 14 */
    len7:
        /*Check if color is white or black */
        /*Check if piece is a valid one */
        /*Get the start and dest positions */
        if (tokenArr[1][0] == 'W' || tokenArr[1][0] == 'B'){
            if (tokenArr[1][1] == 'K' || tokenArr[1][1] == 'Q' ||
                tokenArr[1][1] == 'B' || tokenArr[1][1] == 'N' 
                || tokenArr[1][1] == 'R' || tokenArr[1][1] == 'P'){
                    if ((int)tokenArr[1][2] > 96 && (int)tokenArr[1][2] < 105){
                        if (isdigit(tokenArr[1][3]) && (int)tokenArr[1][3] > 48 && (int)tokenArr[1][3] < 57){
                            if (tokenArr[1][4] == '-'){
                                if ((int)tokenArr[1][5] > 96 && (int)tokenArr[1][5] < 105){
                                    if (isdigit(tokenArr[1][6]) && (int)tokenArr[1][6] > 48 && (int)tokenArr[1][6] < 57){
                                        color = tokenArr[1][0];
                                        piece = tokenArr[1][1];
                                        startC = tokenArr[1][2];
                                        startR = tokenArr[1][3];
                                        destC = tokenArr[1][5];
                                        destR = tokenArr[1][6];
                                        goto doneRead;
                                    }
                                }
                            }
                        }
                    }
                }
        }
        LOG_INFO("Entered move is illegal\n");
        returnStr = illMove;
        goto clearMem;

    doneRead:
        /*
        Example: WPf7-g8xBNyWB\n  ... this has a length of 14 
        'K' for king, 'Q' for queen, 'B' for bishop, 'N' for knight, 'R' for rook, and 'P' for pawn
        color, piece, startC, startR, destC, destR, enemColor, enemPiece, transColor, transPiece
        */
        i=0;
        c=0;

        /* 
        We need checks for:
        -If the color is the correct one for player1
        -If piece is actually present in the column,row
        -If the move to the dest column,row is possible for the peice
            -Check if something exists in that destination, if yes is enemPiece actually there?
            -If nothing exists in destination, check enemColor and enemPiece for \0
            -Check if after this move, the king will not be in check, if not invalid move
        
        -Is the destination at end of board and is a pawn (for white row 8, for black row 1):
            -If upgrade not specified, ILLMOVE\n
            -If destination not at end of board check dest variables for \0


        -Check if the enemy has been put under check and by how many pieces
            -If yes:
                -Can the enemy move out of the check?
                -If only one piece is checking:
                    -Can the enemy block the attacking piece (can't be done with knight or pawn)?
                    -Can the enemy take the attacking piece?
                -If more than one:
                    -Can the king take one of them and exit check?
                -If we answer no to all these it is a checkmate


        We need a function that will take a given square on the board and examine if it can be reached
        by the enemy pieces (this is for check and for if the enemy can take the attacking piece). 

        For blocking, it can only be done when one piece is attacking. Therefore we see if we can the enemy
        can put a piece between the king and attacking piece, also using the previous function.
        */

        //get index for our 8 by 8 board
        startIndex = getIndex(startC, startR);

        //check if the color and type matches for the moving piece
        if (board[startIndex] == NULL){
            LOG_ERROR("Nothing in this index\n");
            returnStr = illMove;
            goto clearMem;
        }

        if ((board[startIndex]->color != *player1) || (*player1 != color) || board[startIndex]->type != piece){
            LOG_ERROR("Color or piece type do not match!\n");
            returnStr = illMove;
            goto clearMem;
        }

        intStartCol = (int)startC - 97;
        intStartRow = (int)startR - 49;

        endIndex = getIndex(destC, destR);

        //Check if the piece can actually move to the destination
        switch (piece)
        {
        case 'K':
            possible = kingMove(intStartCol, intStartRow, endIndex);
            break;
        case 'Q':
            possible = queenMove(intStartCol, intStartRow, endIndex);
            break;
        case 'B':
            possible = bishopMove(intStartCol, intStartRow, endIndex);
            break;
        case 'N':
            possible = knightMove(intStartCol, intStartRow, endIndex);
            break;
        case 'R':
            possible = rookMove(intStartCol, intStartRow, endIndex);
            break;
        case 'P':
            possible = pawnMove(intStartCol, intStartRow, endIndex);
            break;
        default:
            LOG_ERROR("Unknown piece!\n");
            possible = 0;
            break;
        }
        
        if (possible == 0){
            LOG_ERROR("Can't reach destination spot\n");
            returnStr = illMove;
            goto clearMem;
        }

        //Check if the destination contains a same colored piece
        if (board[endIndex] != NULL){
            if (board[startIndex]->color == board[endIndex]->color){
                LOG_ERROR("Attacking same color\n");
                returnStr = illMove;
                goto clearMem;
            }

            if (enemColor == '\0' || enemPiece == '\0'){
                LOG_ERROR("There is a piece that is being captured but wasn't specified\n");
                returnStr = illMove;
                goto clearMem;
            }

            if (board[endIndex]->color != enemColor || board[endIndex]->type != enemPiece){
                LOG_ERROR("Piece being captured is not as specified\n");
                returnStr = illMove;
                goto clearMem;
            }
        }
        else{
            if (enemColor != '\0' || enemPiece != '\0'){
                LOG_ERROR("There isn't a piece that will be captured\n");
                returnStr = illMove;
                goto clearMem;
            }
        }

        //Check if by this move you are putting your king in check
        if (color == WHITE){
            myArray = whitePieces;
            enemArray = blackPieces;
            enemColor = BLACK;
            
        }
        else{
            myArray = blackPieces;
            enemArray = whitePieces;
            enemColor = WHITE;
        }

        // enemyPiecesColor = WHITE;
        // if (myPiecesColor == WHITE){
        //     enemyPiecesColor = BLACK;
        // }

        enemKingLoc = getMyKing(enemColor);
        if (board[enemKingLoc] == NULL || board[enemKingLoc]->type != KING || board[enemKingLoc]->color != enemColor){
            LOG_ERROR("Problem finding enemy king\n");
            returnStr = illMove;
            goto clearMem;
        }

        if (board[endIndex] != NULL){
            board[endIndex]->alive = 0;
        }

        tempPiece = board[endIndex];
        //VERIFY IF THE DESTINATION WAS BEING ATTACKED OR NOT

        board[endIndex] = board[startIndex];
        
        board[endIndex]->index = endIndex;
        board[endIndex]->type = board[startIndex]->type;
        board[startIndex] = NULL;
        myKingLoc = getMyKing(color);

        if (board[myKingLoc] == NULL || board[myKingLoc]->type != KING || board[myKingLoc]->color != color){
            LOG_ERROR("Problem finding friendly king\n");
            returnStr = illMove;
            goto clearMem;
        }

        // gotCheckedInv = amIChecked(myKingLoc, enemArray);

        if (amIChecked(myKingLoc, enemArray)){
            LOG_ERROR("Illegal move, places you under check!\n");
            board[startIndex] = board[endIndex];
            board[startIndex]->index = startIndex;
            board[endIndex] = tempPiece;
            if (board[endIndex] != NULL){
                board[endIndex]->alive = 1;
            }

            returnStr = illMove;
            goto clearMem;
        }

        board[endIndex]->moved = board[endIndex]->moved + 1;

        LOG_INFO("Checking for transformation of pawn stuff\n");

        if (board[endIndex]->type == PAWN){
            if (board[endIndex]->color == WHITE){
                if (endIndex > 55 && endIndex < 64 &&  transColor == color && transColor != '\0' && transPiece != '\0'){
                    board[endIndex]->type = transPiece;
                }
            }
            else{
                if (endIndex > -1 && endIndex < 8 &&  transColor == color && transColor != '\0' && transPiece != '\0'){
                    board[endIndex]->type = transPiece;
                }
            }
        }
        
        LOG_INFO("Seeing if enemy king will be put under attack\n");

        numCheckers = amIChecked(enemKingLoc, myArray);

        LOG_INFO("DONE Seeing if enemy king will be put under attack\n");

        //struct piece dummy;
        if (numCheckers){
            //Is it check or checkmate???
            //Outer loop for all pieces of enemy 
            for (i = 0; i < 16; i++){

                if(notMate){
                    break;
                }

                if (enemArray[i].alive == 0){
                    continue;
                }
                
                enemRow = enemArray[i].index / 8;
                enemCol = enemArray[i].index % 8;

                for (c = 0; c < 64; c++){
                    if (c == enemArray[i].index){
                        continue;
                    }

                    if (board[c] != NULL){
                        if (board[c]->color == enemArray[i].color){
                            continue;
                        }
                    }

                    possible = 0;

                    LOG_INFO("Will enter switch statement now\n");

                    switch (enemArray[i].type)
                    {
                    case 'K':
                        LOG_INFO("Checking Kingmove\n");
                        possible = kingMove(enemCol, enemRow, c);
                        LOG_INFO("Done checking Kingmove\n");
                        break;
                    case 'Q':
                        LOG_INFO("Checking Queenmove\n");
                        possible = queenMove(enemCol, enemRow, c);
                        LOG_INFO("Done checking Queenmove\n");
                        break;
                    case 'B':
                        LOG_INFO("Checking bishopMove\n");
                        possible = bishopMove(enemCol, enemRow, c);
                        LOG_INFO("Done checking bishopMove\n");
                        break;
                    case 'N':
                        LOG_INFO("Checking Knightmove\n");
                        possible = knightMove(enemCol, enemRow, c);
                        LOG_INFO("Done checking Knightmove\n");
                        break;
                    case 'R':
                        LOG_INFO("Checking rookMove\n");
                        possible = rookMove(enemCol, enemRow, c);
                        LOG_INFO("Done checking rookMove\n");
                        break;
                    case 'P':
                        LOG_INFO("Checking pawnMove\n");
                        possible = pawnMove(enemCol, enemRow, c);
                        LOG_INFO("Done checking pawnmove\n");
                        break;
                    default:
                        LOG_ERROR("Unknown piece!\n");
                        break;
                    }

                    LOG_INFO("Finished switch statement\n");

                    if (possible){
                        if (board[enemArray[i].index] == NULL){
                            LOG_ERROR("Something wrong, the enemArray index is empty!\n");
                        }
                        
                        LOG_INFO("The type of enemArrary[i] is: %c", enemArray[i].type);

                        LOG_INFO("The color of enemArrary[i] is: %c", enemArray[i].color);

                        LOG_INFO("The type of board[enemArray[i]] is: %c", board[enemArray[i].index]->type);

                        LOG_INFO("The color of board[enemArray[i]] is: %c", board[enemArray[i].index]->color);

                        LOG_INFO("BEFFORE: the index of board[enemArray[i]] is: %d , or: %d\n", 
                        board[enemArray[i].index]->index, enemArray[i].index);

                        LOG_INFO("BEFFORE: the index of board[c] is:");
                        
                        if (board[c] == NULL){
                            LOG_INFO("NULL\n");
                        }
                        else{
                            LOG_INFO("%d\n", board[c]->index);
                        }

                        //tempPiece set to destination 
                        tempPiece = board[c];
                        if (board[c] != NULL){
                            board[c]->alive = 0;
                        }
                        //Move source piece to destination
                        tempIndex = enemArray[i].index;
                        board[c] = board[enemArray[i].index];
                        if(board[c] == NULL){
                            LOG_ERROR("The board index c is pointing to NULL!\n");
                        }
                        //Make source empty
                        board[enemArray[i].index] = NULL;
                        //Update index
                        board[c]->index = c;

                        enemKingLoc = getMyKing(enemColor);
                        if (amIChecked(enemKingLoc, myArray) == 0){
                            notMate = 1;
                        }

                        board[tempIndex] = board[c];

                        if(board[tempIndex] == NULL){                            
                            LOG_ERROR("The board index of i's piece is pointing to NULL!\n");
                        }

                        board[tempIndex]->index = tempIndex;
                        board[c] = tempPiece;
                        if (board[c] != NULL){
                            board[c]->alive = 1;
                        }

                        LOG_INFO("AFTER: the index of board[enemArray[i]] is: %d , or: %d\n", 
                        board[enemArray[i].index]->index, enemArray[i].index);

                        LOG_INFO("AFTEr: the index of board[c] is:");
                        
                        if (board[c] == NULL){
                            LOG_INFO("NULL\n");
                        }
                        else{
                            LOG_INFO("%d\n", board[c]->index);
                        }

                        if (notMate){
                            break;
                        }
                    }
                }
            }
        }

        LOG_INFO("A regular move\n");

        //Update the char board
        charBoard[startIndex * 2] = '*';
        charBoard[(startIndex * 2) + 1] = '*';
        charBoard[endIndex * 2] = board[endIndex]->color;
        charBoard[(endIndex * 2) + 1] = board[endIndex]->type;

        turn = !turn;

        if (notMate){
            LOG_INFO("A regular check\n");
            returnStr = check;
            goto clearMem;
            }
        else if (numCheckers){
            LOG_INFO("Checkmate!\n");
            gameOver = 1;
            returnStr = mate;
            goto clearMem;
        }   

        returnStr = ok;
        goto clearMem;

    case3:
        if (gameOver){
            returnStr = noGame;
            goto clearMem;
        }

        if (*player1 == wb[turn]){
            returnStr = oot;
            goto clearMem;
        }

        LOG_INFO("CPU's turn!");

        //Loop through the array belonging to the CPU
        //Loop through the board
        //Check for a move that can be made without being put under check
        //Then determine enemy check/checkmate

        turn = !turn;

        goto clearMem;

    case4:
        LOG_INFO("We are in case 4\n");

    clearMem:
        for (i = 0; i < 10; i++){
            if (tokenArr[i] != NULL){
                kfree(tokenArr[i]);
            }
            else{
                break;
            }
        }
        kfree(tokenArr);
        kfree(kernelBuff);

        calcSize();

        /* Unlock */
        mutex_unlock(&lock);

        //Tell whoever called write function, we tell them how many bytes we wrote in our file
        return length;
}

int chess_release(struct inode *pinode, struct file *pfile){
    LOG_INFO("In function %s\n", __FUNCTION__);
    
    //We return 0 because we don't have any error
    return 0;
}

void calcSize(){
    buffer_size = 0;
    while (returnStr[buffer_size] != '\n'){
        buffer_size++;
    }
    buffer_size++;
}

void setupColors(struct piece array[], int color){
    int i, setColor;

    if (color == 0){
        setColor = WHITE;
    }
    else{
        setColor = BLACK;
    }

    for (i = 0; i < 8; i++){
        array[i].color = setColor;
        array[i].alive = 1;
        if (i == 0 || i == 7){
            array[i].type = ROOK;
        }
        else if (i == 1 || i == 6){
            array[i].type = KNIGHT;
        }
        else if (i == 2 || i == 5){
            array[i].type = BISHOP;
        }
        else if (i == 3){
            if (setColor == WHITE){
                array[i].type = QUEEN;
            }
            else{
                array[i].type = KING;
            }
        }
        else{
            if (setColor == WHITE){
                array[i].type = KING;
            }
            else{
                array[i].type = QUEEN;
            }
        }
    }

    for (i = 8; i < 16; i++){
        array[i].color = setColor;
        array[i].type = PAWN;
        array[i].moved = 0;
        array[i].alive = 1;
    }
}

void setupBoard(struct piece whitePieces[], struct piece blackPieces[]){
    int i;

    for (i = 0; i < 16; i++){
        board[i] = &whitePieces[i];
        board[i]->index = i;
        charBoard[i * 2] = whitePieces[i].color;
        charBoard[(i * 2) + 1] = whitePieces[i].type;

        board[63 - i] = &blackPieces[i];
        board[63 - i]->index = 63 - i;
        charBoard[(63 - i) * 2] = blackPieces[i].color;
        charBoard[((63 - i) * 2) + 1] = blackPieces[i].type;
    }
}

int getIndex(char col, char row){
    int intcol, introw, index;

    intcol = (int)col - 97;
    introw = (int)row - 49;

    index = intcol + (8 * introw);

    return index;
}

//We need start and end indicies, the row and column
int kingMove(int startCol, int startRow, int endIndex){
    int i, j;
    int tempCol, tempRow;
    
    for (i = -1; i < 2; i++){
        for (j = -1; j < 2; j++){
            if (i == 0 && j == 0){
                continue;
            }
            tempRow = startRow + i;
            tempCol = startCol + j;
            if ((8*tempRow + tempCol) == endIndex){
                return 1;
            }
        }  
    }
    return 0;
}

//We need start and end indicies, the row and column
int queenMove(int startCol, int startRow, int endIndex){

    if (kingMove(startCol, startRow, endIndex) == 1){
        return 1;
    }

    if (bishopMove(startCol, startRow, endIndex)){
        return 1;
    }

    if (rookMove(startCol, startRow, endIndex)){
        return 1;
    }

    return 0;
}

int bishopMove(int startCol, int startRow, int endIndex){
    //Lower left diagonal
    if (planeMove(startCol - 1, startRow - 1, endIndex, -1, -1)){
        return 1;
    }

    //Lower right diagonal
    if (planeMove(startCol + 1, startRow - 1, endIndex, 1, -1)){
        return 1;
    }

    //Upper left diagonal
    if (planeMove(startCol - 1, startRow + 1, endIndex, -1, 1)){
        return 1;
    }

    //Upper right diagonal
    if (planeMove(startCol + 1, startRow + 1, endIndex, 1, 1)){
        return 1;
    }  

    return 0;
}

int knightMove(int startCol, int startRow, int endIndex){
    //L shaped check code
    int movePos;

    //lying L lower left
    movePos = 8*(startRow - 1) + (startCol - 2);
    if (movePos == endIndex){
        return 1;
    }

    //lying L lower right
    movePos = 8*(startRow - 1) + (startCol + 2);
    if (movePos == endIndex){
        return 1;
    }

    //standing L lower left
    movePos = 8*(startRow - 2) + (startCol - 1);
    if (movePos == endIndex){
        return 1;
    }

    //standing L lower right
    movePos = 8*(startRow - 2) + (startCol + 1);
    if (movePos == endIndex){
        return 1;
    }

    //lying L upper left
    movePos = 8*(startRow + 1) + (startCol - 2);
    if (movePos == endIndex){
        return 1;
    }

    //lying L upper right
    movePos = 8*(startRow + 1) + (startCol + 2);
    if (movePos == endIndex){
        return 1;
    }

    //standing L upper left
    movePos = 8*(startRow + 2) + (startCol - 1);
    if (movePos == endIndex){
        return 1;
    }

    //standing L upper right
    movePos = 8*(startRow + 2) + (startCol + 1);
    if (movePos == endIndex){
        return 1;
    }

    return 0;
}

int rookMove(int startCol, int startRow, int endIndex){
    //Left
    if (planeMove(startCol - 1, startRow, endIndex, -1, 0)){
        return 1;
    }   

    //Down
    if (planeMove(startCol, startRow - 1, endIndex, 0, -1)){
        return 1;
    }  

    //Right
    if (planeMove(startCol + 1, startRow, endIndex, 1, 0)){
        return 1;
    }  

    //Up
    if (planeMove(startCol, startRow + 1, endIndex, 0, 1)){
        return 1;
    }  

    return 0;
}

int pawnMove(int startCol, int startRow, int endIndex){
    int currPos, movePos;
    //First we check if it is attacking or simply moving
    currPos = 8*startRow + startCol;

    //If the pawn is attacking an enemy piece
    if (board[endIndex] != NULL){

        movePos = 8*(startRow + 1) + (startCol - 1);
        if (movePos == endIndex){
            return 1;
        }

        movePos = 8*(startRow + 1) + (startCol + 1);
        if (movePos == endIndex){
            return 1;
        }
    }
    else{
        if (board[currPos]->moved == 0){
            movePos = 8*(startRow + 2) + startCol;
            if (movePos == endIndex){
                //So that we change moved to 1
                return 3;
            }
        }

        movePos = 8*(startRow + 1) + startCol;
        if (movePos == endIndex){
            return 2;
        }
    }

    return 0;
}

int planeMove(int xPos, int yPos, int endIndex, int xAdd, int yAdd){
    int currIndex;
    //Base case: if xPos + yPos*8 == endPos or if 
    if (xPos < 0 || xPos > 7 || yPos < 0 || yPos > 7){
        return 0;
    }
    //Calculate the index/board position currently
    currIndex = 8*yPos + xPos;
    
    //If we hit a piece before reaching the destination, return 0
    if (board[currIndex] != NULL){
        if (currIndex == endIndex){
            return 1;
        }
        return 0;
    }
    else{
        if (currIndex == endIndex){
            return 1;
        }
    }

    return planeMove(xPos + xAdd, yPos + yAdd, endIndex, xAdd, yAdd);
}

int getMyKing(myPiecesColor){
    if (myPiecesColor == WHITE){
        return whitePieces[4].index;
    }

    return blackPieces[3].index;
}

int amIChecked(int underAttack, struct piece* enemArray){
    int i, col, row;
    char pieceType;
    int possible = 0;

    for (i = 0; i < 16; i++){

        if (enemArray[i].alive == 0){
            continue;
        }
        
        pieceType = enemArray[i].type;

        row = enemArray[i].index / 8;
        col = enemArray[i].index % 8;     

        switch (pieceType)
        {
        case 'K':
            possible += kingMove(col, row, underAttack);
            break;
        case 'Q':
            possible += queenMove(col, row, underAttack);
            break;
        case 'B':
            possible += bishopMove(col, row, underAttack);
            break;
        case 'N':
            possible += knightMove(col, row, underAttack);
            break;
        case 'R':
            possible += rookMove(col, row, underAttack);
            break;
        case 'P':
            possible += pawnMove(col, row, underAttack);
            break;
        default:
            LOG_ERROR("Unknown piece!\n");
            break;
        }
    }

    if (possible == 0){
        LOG_INFO("The King is safe\n");
    }

    return possible;
}
