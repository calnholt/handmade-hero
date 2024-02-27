# include "handmade.h"

internal void 
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffest, int GreenOffset) {
  uint8 *Row = (uint8 *)Buffer->Memory;  
  for(int Y = 0; Y < Buffer->Height; ++Y) {
    uint32 *Pixel = (uint32 *)Row;
    for(int X = 0; X < Buffer->Width; ++X) {
      /*
        MEMORY ORDER:      RR GG BB xx
        LOADED IN:         xx BB GG RR
        WANTED:            xx RR GG BB
        WIN MEMORY ORDER:  BB GG RR xx
      */
      uint8 Blue = (X + BlueOffest);
      uint8 Green = (Y + GreenOffset);
      *Pixel++ = (Green << 8) | Blue;
    }
    Row += Buffer->Pitch;
    // Row = (uint8 *)Pixel; <-- also works
  }
}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset) {
  RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}