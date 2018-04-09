#include"main.h"
#include<time.h>
#include<stdlib.h>

class State {

public:
  board_t board;

  // copy and assignment operators should perform a DEEP clone of the given state
  State(const State& other);
  State& operator = (const State& other);

  // whether or not this state is terminal (reached end)
  bool is_terminal() const{
    int move;

    for(move = 0; move < 4; move++) {
        if(execute_move(move, board) != board)
            return false;
    }
    return true;
  }

  //  agent id (zero-based) for agent who is about to make a decision
  int agent_id() const{return 0;}

  // apply action to state
  void apply_action(const Action& action){
    board = execute_move(action, board);
  }

  // return possible actions from this state
  void get_actions(std::vector<Action>& actions) const{
    actions.resize(0);
    int move;
    for(move = 0; move < 4; move++) {
        if(execute_move(move, board) != board){
          Action action = move;
          actions.push_back(action);
        }
    }
  }

  // get a random action, return false if no actions found
  bool get_random_action(Action& action) const{
    std::vector<Action> actions;
    get_actions(actions);
    if(actions.size()==0){
      return false;
    }
    else{
      srand(time(NULL));
      action = actions[rand()%actions.size()];
    }
    return true;
  }

  // evaluate this state and return a vector of rewards (for each agent)
  const std::vector<float> evaluate() const{
    std::vector<float> rewards(1);
    if(is_terminal()){
      int score = max_tile(board);
      if(score>5){
        rewards[0] = 1;
        return rewards;
      }
    }
    rewards[0] = 0;
    return rewards;
  }

  // return state as string (for debug purposes)
  std::string to_string() const{
    return std::to_string(board);
  }
};
