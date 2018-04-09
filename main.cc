#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <main.h>
#include <ofxMSAmcts.h>
#include <State.h>

filename = "/Users/Pragjnesh/Documents/2048RL/network.tf";
static row_t row_left_table[65536];
static row_t row_right_table[65536];
static board_t col_up_table[65536];
static board_t col_down_table[65536];
static int score_table[65536];

void init_tables(){
  for (unsigned row = 0; row < 65536; ++row) {
      unsigned line[4] = {
              (row >>  0) & 0xf,
              (row >>  4) & 0xf,
              (row >>  8) & 0xf,
              (row >> 12) & 0xf
      };

      // Score
      int score = 0.0f;
      for (int i = 0; i < 4; ++i) {
          int rank = line[i];
          if (rank >= score) {
              // the score is the maximum tile in the row
              score = rank;
          }
      }
      score_table[row] = score;

      // execute a move to the left
      for (int i = 0; i < 3; ++i) {
          int j;
          for (j = i + 1; j < 4; ++j) {
              if (line[j] != 0) break;
          }
          if (j == 4) break; // no more tiles to the right

          if (line[i] == 0) {
              line[i] = line[j];
              line[j] = 0;
              i--; // retry this entry
          } else if (line[i] == line[j]) {
              if(line[i] != 0xf) {
                  /* Pretend that 32768 + 32768 = 32768 (representational limit). */
                  line[i]++;
              }
              line[j] = 0;
          }
      }

      row_t result = (line[0] <<  0) |
                     (line[1] <<  4) |
                     (line[2] <<  8) |
                     (line[3] << 12);
      row_t rev_result = reverse_row(result);
      unsigned rev_row = reverse_row(row);

      row_left_table [    row] =                row  ^                result;
      row_right_table[rev_row] =            rev_row  ^            rev_result;
      col_up_table   [    row] = unpack_col(    row) ^ unpack_col(    result);
      col_down_table [rev_row] = unpack_col(rev_row) ^ unpack_col(rev_result);
  }
}

// void init_network(){
//   if(/*Filename exists*/){
//     //Load the pre-trained network from Filename
//   }
//   else {
//     // Initialize a fresh network
//   }
// }
//
// void save_network(){
//   if(/*Filename exists */){
//     // Save the network under FilenameX
//   }
//   else {
//     // Save the network under Filename1
//   }
// }

static int count_empty(board_t x)
{
    x |= (x >> 2) & 0x3333333333333333ULL;
    x |= (x >> 1);
    x = ~x & 0x1111111111111111ULL;
    // At this point each nibble is:
    //  0 if the original nibble was non-zero
    //  1 if the original nibble was zero
    // Next sum them all
    x += x >> 32;
    x += x >> 16;
    x += x >>  8;
    x += x >>  4; // this can overflow to the next nibble if there were 16 empty positions
    return x & 0xf;
}

static inline int max_tile(board_t board)
{
  int max0 = score_table[((board >>  0) & ROW_MASK)];
  int max1 = score_table[((board >> 16) & ROW_MASK)];
  int max2 = score_table[((board >> 32) & ROW_MASK)];
  int max3 = score_table[((board >> 48) & ROW_MASK)];
  return std::max(std::max(max0,max1),std::max(max2,max3));
}

// Transpose rows/columns in a board:
//   0123       048c
//   4567  -->  159d
//   89ab       26ae
//   cdef       37bf
static inline board_t transpose(board_t x)
{
    board_t a1 = x & 0xF0F00F0FF0F00F0FULL;
    board_t a2 = x & 0x0000F0F00000F0F0ULL;
    board_t a3 = x & 0x0F0F00000F0F0000ULL;
    board_t a = a1 | (a2 << 12) | (a3 >> 12);
    board_t b1 = a & 0xFF00FF0000FF00FFULL;
    board_t b2 = a & 0x00FF00FF00000000ULL;
    board_t b3 = a & 0x00000000FF00FF00ULL;
    return b1 | (b2 >> 24) | (b3 << 24);
}

static board_t draw_tile() {
    return (unif_random(10) < 9) ? 1 : 2;
}

static board_t insert_tile_rand(board_t board, board_t tile) {
    int index = unif_random(count_empty(board));
    board_t tmp = board;
    while (true) {
        while ((tmp & 0xf) != 0) {
            tmp >>= 4;
            tile <<= 4;
        }
        if (index == 0) break;
        --index;
        tmp >>= 4;
        tile <<= 4;
    }
    return board | tile;
}

static board_t initial_board() {
    board_t board = draw_tile() << (4 * unif_random(16));
    return insert_tile_rand(board, draw_tile());
}

static inline board_t execute_move_0(board_t board) {
    board_t ret = board;
    board_t t = transpose(board);
    ret ^= col_up_table[(t >>  0) & ROW_MASK] <<  0;
    ret ^= col_up_table[(t >> 16) & ROW_MASK] <<  4;
    ret ^= col_up_table[(t >> 32) & ROW_MASK] <<  8;
    ret ^= col_up_table[(t >> 48) & ROW_MASK] << 12;
    return ret;
}

static inline board_t execute_move_1(board_t board) {
    board_t ret = board;
    board_t t = transpose(board);
    ret ^= col_down_table[(t >>  0) & ROW_MASK] <<  0;
    ret ^= col_down_table[(t >> 16) & ROW_MASK] <<  4;
    ret ^= col_down_table[(t >> 32) & ROW_MASK] <<  8;
    ret ^= col_down_table[(t >> 48) & ROW_MASK] << 12;
    return ret;
}

static inline board_t execute_move_2(board_t board) {
    board_t ret = board;
    ret ^= board_t(row_left_table[(board >>  0) & ROW_MASK]) <<  0;
    ret ^= board_t(row_left_table[(board >> 16) & ROW_MASK]) << 16;
    ret ^= board_t(row_left_table[(board >> 32) & ROW_MASK]) << 32;
    ret ^= board_t(row_left_table[(board >> 48) & ROW_MASK]) << 48;
    return ret;
}

static inline board_t execute_move_3(board_t board) {
    board_t ret = board;
    ret ^= board_t(row_right_table[(board >>  0) & ROW_MASK]) <<  0;
    ret ^= board_t(row_right_table[(board >> 16) & ROW_MASK]) << 16;
    ret ^= board_t(row_right_table[(board >> 32) & ROW_MASK]) << 32;
    ret ^= board_t(row_right_table[(board >> 48) & ROW_MASK]) << 48;
    return ret;
}

/* Execute a move. */
static inline board_t execute_move(int move, board_t board) {
    switch(move) {
    case 0: // up
        return execute_move_0(board);
    case 1: // down
        return execute_move_1(board);
    case 2: // left
        return execute_move_2(board);
    case 3: // right
        return execute_move_3(board);
    default:
        return ~0ULL;
    }
}

void get_move(board_t board){
  State state;            // contains the current state, it must comply with the State interface
  Action action;          // contains an action that can be applied to a State, and bring it to a new State
  state.board = board;
  UCT<State, Action> uct; // Templated class. Builds a partial decision tree and searches it with UCT MCTS

  // OPTIONAL init uct params
  uct.uct_k = sqrt(2);
  uct.max_millis = 0;
  uct.max_iterations = 100;
  uct.simulation_depth = 5;

  action = uct.run(state);
}

void play_game(){
  board_t board = initial_board();
  int moveno = 0;

  while(1) {
      int move;
      board_t newboard;

      for(move = 0; move < 4; move++) {
          if(execute_move(move, board) != board)
              break;
      }
      if(move == 4)
          break; // no legal moves

      printf("\nMove #%d\n", ++moveno);

      move = get_move(board);
      if(move < 0)
          break;

      // Maybe remove if get_move is smart enough not to return any illegal moves
      newboard = execute_move(move, board);
      if(newboard == board) {
          printf("Illegal move!\n");
          moveno--;
          continue;
      }

      board_t tile = draw_tile();
      board = insert_tile_rand(newboard, tile);
  }

  print_board(board);
  printf("\nGame over.\n");
  // Play game using network and save results
  // Save the game somewhere as a linked-list maybe
}

// void add_game(){
//   // Add the game to a compilation of games
// }
//
// void make_dataset(){
//   // Structure the data so that it can be fed into the graph for training
// }
//
// void train_network(){
//   // Train the network using the dataset created
// }

void self_play(int niter, int batch_size){
  for(int i=0;i<niter;i++){
    for(int j=0;j<batch_size;j++){
      play_game();
      add_game();
    }
    make_dataset();
    train_network();
  }
}

void main() {
  init_tables();
  init_network();
  self_play(niter,batch_size);
  save_network();
}
