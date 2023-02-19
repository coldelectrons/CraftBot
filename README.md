CraftBot
-------------

Experimental Minecraft bot using BotCraft

Only works for whatever BotCraft supports, which is currently Minecraft JE (up to 1.19.3, as of 20230219).

## Planned Features
Make tasks and BT subtrees for:
 - [ ] crop harvesting and planting
 - [ ] trading
 - [ ] attending to item farms
 - [ ] load and build designs, geometric shapes, etc
 - [ ] combat and evasion

My high-level aim is to be able to specify some goal, and the bot performs something like an A-star search of actions to achieve that goal.

My highest-pie-in-the-sky aim is a paratrooper engineer - to be able to drop the bot in a SMP game and have it survey, build a base, build various farms, and build up a stash of materials.

# Build instructions:
~~~bash
mkdir build
cd build
cmake ..
make -j
cd ../target
./minion
~~~

