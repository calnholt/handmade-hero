#if !defined(HANDMADE_H)

#define internal static
typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

// services that the game provides to the platform layer

// input, bitmap buffer to use, sound buffer to use,
// (this may get expanded in the future - sound on separate thread, etc)
struct game_offscreen_buffer {
  void *Memory;
  int Width;
  int Height;
  int Pitch;
};
internal void GameUpdatedateAndRender(game_offscreen_buffer);

// services that the game provides to the platform layer

#define HANDMADE_H
#endif