project2-part2-kodekoala

This is a human player vs. CPU character device chess implementation. The module can be loaded and unloaded from the linux kernel, and interacted with using the /dev/chess file it creates. In this implementation, the CPU's moves are not that all focused on winning, but rather on being valid. The module accepts human input of a certain format as described in the project 2 description, and then verifies that the move is valid before performing it. For both the human player and CPU, moves must not allow the king to become/stay in check, and they must only be to positions on the board that are either empty or occupied by an enemy. Additionally, pieces must abide by their respective movement capabilities (for example kings can only move 1 spot in any direction).

The module was tested using the test program provided by the CMSC 421 TA John for valid and invalid moves alike, checks, and checkmates. It seems to work without any issues or bugs.
