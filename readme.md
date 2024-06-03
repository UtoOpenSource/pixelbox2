# Pixelbox 2
The new version of [pixelbox](https://github.com/UtoOpenSource/pixelbox).   Infinite sandbox gae
**STILL IN PROTOTYPING STAGE!**   

In this version **ALREADY** Done this things :
- Will be rewritten on c++
- GUI is separated from game logic (kinda)
- Rewritten profiler
- Use SDL instead of raylib, ImGUI instead of raygui
     
This new version **WILL** include features like :
- Rewritten particle physic calculations definition
- Improved rendering
- Multithreaded world generation
- Multithreaded world update calculation
     
**In the far future** theese fratures are planned :
- Entity system
- Server and client-side modifications (using luau)
- Server-side assets (textures, sounds, scripts, mods)
- Particles sounds
- Better UI, Background music
- Playable and replayable core game :
  - Achievements and achievement system
	- World Leveling Up: You need to sacrifize some amount of resources to improve your world
	- Save information about laoded chunks when exiting the game. Load all them back when loading again, and only then start physic simulation (REQUIRES A LOT OF TESTING, VALUE-TWEAKING!)
- Improve Out Of Memory stability

# REpo structure
repo strutures is :
- `external` - all external dependencies, used in any modules, not maintained by pixelbox team.
- `engine` - core game systems
- `base`  - basic C++ delarations/classes/methods used everywhere.
- `client` - TODO rename to client 
