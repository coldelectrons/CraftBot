#pragma once

#include <string>
#include <random>
#include <chrono>

#include "botcraft/AI/BehaviourClient.hpp"
#include "botcraft/AI/Status.hpp"

/// @brief Check condition
/// @param client The client performing the action
/// @param radius The max distance to search blocks
/// @return Always return Success
Botcraft::Status InitializeFarmBlocks(Botcraft::BehaviourClient& client, const int radius = 50);

/// @brief Take item in chest until there are N in inventory
/// @param client The client performing the action
/// @param item_name The item to take
/// @param N Number in inventory
/// @return Success if N in inventory, Failure otherwise
Botcraft::Status TakeFromChest(Botcraft::BehaviourClient& client, const std::string& item_name, const int N);

/// @brief Copy a random element of a std::vector<T> to a blackboard location
/// @tparam T Type of the element
/// @param client The client performing the action
/// @param src Blackboard source vector location
/// @param dst Blackboard single element destination
/// @return Always return success
template<typename T>
Botcraft::Status CopyRandomFromVectorBlackboardData(Botcraft::BehaviourClient& client, const std::string& src, const std::string& dst)
{
    Botcraft::Blackboard& blackboard = client.GetBlackboard();

    const std::vector<T>& source = blackboard.Get<std::vector<T>>(src);
    if (source.size() == 1)
    {
        blackboard.Set<T>(dst, source[0]);
        return Botcraft::Status::Success;
    }

    std::mt19937 random_engine(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    blackboard.Set<T>(dst, source[std::uniform_int_distribution<>(0, static_cast<int>(source.size()) - 1)(random_engine)]);

    return Botcraft::Status::Success;
}

/// @brief Break carrots/potatoes, collect the items, replant
/// @param blocks_pos_blackboard Blackboard address of the vector of positions of blocks
/// @param item_name Name of the item we want to collect
/// @return Success if blocks are broken, items gathered and crops replanted, Failure otherwise
Botcraft::Status CollectCropsAndReplant(Botcraft::BehaviourClient& client, const std::string& blocks_pos_blackboard, const std::string& item_name);

/// @brief Use the cactus to destroy any of these items in the inventory
/// @param client The client performing the action
/// @param item_name The item to remove from the inventory
/// @return Success if no more items in inventory, Failure otherwise
Botcraft::Status DestroyItems(Botcraft::BehaviourClient& client, const std::string& item_name);
