# Pixelbox 2
The new version of [pixelbox](https://github.com/UtoOpenSource/pixelbox).   Infinite sandbox game
**STILL IN PROTOTYPING STAGE!**   

In this version **ALREADY** Done this things :
- Will be rewritten on c++
- GUI is separated from game logic (kinda)
- Rewritten profiler
- Use SDL instead of raylib, ImGUI instead of raygui

**In development**:
- All game logic, world rendering... everything

This new version **WILL** include features like :
- Rewritten particle physic calculations definition
- Improved rendering
- Multithreaded world generation
- Multithreaded world update calculation

# REpo structure
repo strutures is :
- `external` - all external dependencies, used in many modules, ~~not maintained by pixelbox team~~.
- `engine` - core game systems
- `client` - game client, world rendering and etc.
