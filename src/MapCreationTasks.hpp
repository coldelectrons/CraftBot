#pragma once

#include <string>

#include "botcraft/AI/BehaviourTree.hpp"
#include "botcraft/AI/BehaviourClient.hpp"
#include "botcraft/Game/Vector3.hpp"

/// @brief Check all blocks nearby for chests. Save the found positions in World.ChestsPos
/// @param c The client performing the action
/// @return Always return Success
Botcraft::Status GetAllChestsAround(Botcraft::BehaviourClient& c);

/// @brief Search in nearby chests for a stack of food, put it in the inventory when found.
/// @param c The client performing the action
/// @param food_name The food item name
/// @return Success if the given food is in inventory, Failure otherwise
Botcraft::Status GetSomeFood(Botcraft::BehaviourClient& c, const std::string& food_name);

/// @brief Get list of blocks in the inventory (ignoring the offhand), store it in the blackboard at Inventory.block_list
/// @param c The client performing the action
/// @return Success if at least one block, Failure otherwise
Botcraft::Status GetBlocksAvailableInInventory(Botcraft::BehaviourClient& c);

/// @brief Take or deposit items in nearby chests
/// @param c The client performing the action
/// @param food_name Food item you don't want to take/deposit
/// @param take_from_chest If true, take items from chests, if false deposit items
/// @return Success if all the items were deposited/the inventory is full, Failure otherwise
Botcraft::Status SwapChestsInventory(Botcraft::BehaviourClient& c, const std::string& food_name, const bool take_from_chest);

/// @brief Read block list in blackboard at Inventory.block_list and fill in NextTask.action, NextTask.pos, NextTask.face and NextTask.item
/// @param c The client performing the action
/// @return success if a task was found, failure otherwise
Botcraft::Status FindNextTask(Botcraft::BehaviourClient& c);

/// @brief Read the blackboard NextTask.XXX values, and perform the given task
/// @param c The client performing the action
/// @return Success if the task was correctly executed, failure otherwise
Botcraft::Status ExecuteNextTask(Botcraft::BehaviourClient& c);

/// @brief Check if the whole structure is built, check in the blackboard for CheckCompletion.(print_details, print_errors and full_check) to know if details should be displayed in the console
/// @param c The client performing the action
/// @return Success if the structure is fully built, failure otherwise
Botcraft::Status CheckCompletion(Botcraft::BehaviourClient& c);

/// @brief Write a message in the console, prefixed with bot name
/// @param c The client performing the action
/// @param msg The string to write in the console
/// @return Always return success
Botcraft::Status WarnConsole(Botcraft::BehaviourClient& c, const std::string& msg);

/// @brief Loads a Structure NBT file (unzipped) and store the target structure in the blackboard of the given client
/// @param c The client performing the action
/// @param path The path to the unzipped NBT file
/// @param offset Starting offset position to build the structure
/// @param temp_block Minecraft item/block name used as scafholding block in the NBT
/// @param log_info Whether or not the loading information should be logged
/// @return Success if the file was correctly loaded, failure otherwise
Botcraft::Status LoadStructureNBT(Botcraft::BehaviourClient& c, const std::string& path, const Botcraft::Position& offset, const std::string& temp_block, const bool log_info);

/// @brief Loads a Schematic NBT file (unzipped) and store the target structure in the blackboard of the given client
/// @param c The client performing the action
/// @param path The path to the unzipped NBT file
/// @param offset Starting offset position to build the structure
/// @param temp_block Minecraft item/block name used as scafholding block in the NBT
/// @param log_info Whether or not the loading information should be logged
/// @return Success if the file was correctly loaded, failure otherwise
Botcraft::Status LoadSchematicNBT(Botcraft::BehaviourClient& c, const std::string& path, const Botcraft::Position& offset, const std::string& temp_block, const bool log_info);

/// @brief Loads an image file, computes the target structure and stores it in the blackboard of the given client
/// @param c The client performing the action
/// @param path The path to the greyscale image
/// @param offset Starting offset position to build the structure
/// @param log_info Whether or not the loading information should be logged
/// @return Success if the file was correctly loaded, failure otherwise
Botcraft::Status LoadHeightField(Botcraft::BehaviourClient& c, const std::string& path, const Botcraft::Position& offset, const bool log_info);

/// @brief Enqueue tasks to create/dig a circle
/// @param c The client performing the action
/// @param center The center point of the circle
/// @param radius The radius of the circle
/// @param thickness The thickness of the circle
/// @param outwards Bool whether the thickness is inwards or outwards
/// @param height The height of the circle
/// @param material The material to use
/// @param log_info Bool enable logging
/// @return Success if the tasks were enqueued, Failure otherwise
Botcraft::Status CreateCircle(Botcraft::BehaviourClient& c, const Botcraft::Position& center, float radius, float thickness, bool outwards, float height, const std::string& material);

/// @brief Enqueue tasks to create/dig a line
/// @param c The client performing the action
/// @param start The start point of the line
/// @param end The end point of the line
/// @param width The width of the line
/// @param thickness The thickness of the line
/// @param upwards Bool whether the thickness is upwards or downwards
/// @param material The material to use
/// @return Success if the tasks were enqueued, Failure otherwise
//Botcraft::Status CreateLine(Botcraft::BehaviourClient& c, const Botcraft::Position& start, const Botcraft::Position& end, float width, float thickness, bool upwards, const std::string& material);

/// @brief Enqueue tasks to cover an area (like with transparent blocks to prevent spawn)
/// @param c The client performing the action
/// @param center The center point of the circle
/// @param radius The radius of the circle
/// @param material The material to use
/// @return Success if the tasks were enqueued, Failure otherwise
// Botcraft::Status CoverCircle(Botcraft::BehaviourClient& c, const Botcraft::Position& center, float radius, const std::string& material);

