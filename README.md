# Retro-Bundle


> A basic retro game bundle for NeurOS base programs 
> Idea is simple...distracting youself with some all time classic games when you're bored 

# Table Of Contents 

- [Games](#games)
- [Upcoming Games](#up-coming-games)
- [Game Details](#game-details)



## Games 

- Pong
- Breakout
- Tetris


## Up-coming Games

- Galaga


## Game Details

### Tetris

Build with a C compiler, raylib, and pkg-config installed:

```sh
cc -std=c11 -Wall -Wextra -Wpedantic games/tetris.c -o tetris $(pkg-config --cflags --libs raylib) -lm
./tetris
```
