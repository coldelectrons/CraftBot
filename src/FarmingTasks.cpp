#include "FarmingTasks.hpp"
#include <set>

#include "botcraft/Game/AssetsManager.hpp"
#include "botcraft/Game/Entities/EntityManager.hpp"
#include "botcraft/Game/Entities/LocalPlayer.hpp"
#include "botcraft/Game/Entities/entities/item/ItemEntity.hpp"
#include "botcraft/Game/Entities/entities/npc/VillagerEntity.hpp"
#include "botcraft/Game/Inventory/InventoryManager.hpp"
#include "botcraft/Game/Inventory/Window.hpp"
#include "botcraft/Game/World/World.hpp"
#include "botcraft/Utilities/Logger.hpp"

#include "botcraft/AI/Tasks/AllTasks.hpp"

using namespace Botcraft;
using namespace ProtocolCraft;

// Find crops in a radius
// Find a designated spot to dispose of bad items (ie poisounous potatoes)
// Find a designated storage for output

Status InitializeHarvestBlocks(BehaviourClient& client,
                               const int radius,
                               const std::set<std::string>& croplist) {
  Blackboard& b = client.GetBlackboard();

  Position bed_position;
  Position idle_position;
  Position output_shulker_position;
  Position disposal_position;
  Position disposal_standing_position;
  Position compostor_position;

  std::map<std::string, std::vector<Position>> crops;
  std::vector<Position> signs;
  std::vector<Position> chests;

  Position my_pos;
  {
    std::lock_guard<std::mutex> lock_localplayer(
        client.GetEntityManager()->GetLocalPlayer()->GetMutex());
    my_pos = client.GetEntityManager()->GetLocalPlayer()->GetPosition();
  }

  {
    std::shared_ptr<World> world = client.GetWorld();
    std::lock_guard<std::mutex> lock_world(world->GetMutex());
    Position current_pos;
    for (int x = -radius; x < radius + 1; ++x) {
      current_pos.x = my_pos.x + x;
      for (int y = std::max(-radius, world->GetMinY());
           y < std::min(world->GetMinY() + world->GetHeight(), radius + 1);
           ++y) {
        current_pos.y = my_pos.y + y;
        for (int z = -radius; z < radius + 1; ++z) {
          current_pos.z = my_pos.z + z;

          const Block* block = world->GetBlock(current_pos);
          if (!block) {
            continue;
          }
          const std::string& block_name = block->GetBlockstate()->GetName();
          if (croplist.contains(block_name)) {
            crops[block_name].push_back(current_pos);
            LOG_INFO(block_name << " found at: " << current_pos << "!");
          }
          // We are looking for signs
          // minecraft:oak_sign[rotation=9,waterlogged=false]{Color:"black",GlowingText:0b,Text1:'{"text":"MINION"}',Text2:'{"text":"TEMP
          // STORAGE"}',Text3:'{"text":""}',Text4:'{"text":""}'}
          else if (block_name.ends_with("_sign")) {
            signs.push_back(current_pos);
            LOG_INFO("Sign found at: " << current_pos << "!");
          }
        }
      }
    }
    for (auto& pos : signs) {
      auto bedsp = world->GetBlockEntityData(pos);
      if (bedsp) {
        if (bedsp->contains("Text1")) {
          if ((*bedsp)["Text1"].get<NBT::TagString>() == "{\"text\":\"MINION\"}") {
            LOG_INFO("MINION Sign found at: " << pos << "!");
            // auto text2 = (*bedsp)["Text2"].get<NBT::TagString>();
          }
        }
      }
    }
  }

  // Sort crop positions
  for (auto& pv : crops) {
    std::sort(pv.second.begin(), pv.second.end(),
              [&](const Position& a, const Position& b) {
                return a.SqrDist(stone_standing_position) <
                       b.SqrDist(stone_standing_position);
              });
  }
  b.Set("HarvestBot.disposal_position", disposal_position);
  b.Set("HarvestBot.disposal_standing_position", disposal_standing_position);

  b.Set("HarvestBot.crop_positions", crops);

  b.Set("HarvestBot.initialized", true);

  return Status::Success;
}

Status CleanChest(BehaviourClient& client,
                  const std::string& chest_pos_blackboard,
                  const std::string& item_to_keep) {
  Blackboard& blackboard = client.GetBlackboard();
  std::shared_ptr<InventoryManager> inventory_manager =
      client.GetInventoryManager();

  const Position& pos = blackboard.Get<Position>(chest_pos_blackboard);
  const Position& cactus_standing_pos =
      blackboard.Get<Position>("HarvestBot.disposal_standing_position");
  const Position& cactus_pos =
      blackboard.Get<Position>("HarvestBot.disposal_position");

  bool has_wrong_items = true;
  while (has_wrong_items) {
    if (OpenContainer(client, pos) == Status::Failure) {
      LOG_WARNING("Failed to open container for cleaning");
      return Status::Failure;
    }
    for (int i = 0; i < 100; ++i) {
      client.Yield();
    }

    short container_id;
    std::shared_ptr<Window> container;
    {
      std::lock_guard<std::mutex> inventory_manager_lock(
          inventory_manager->GetMutex());
      container_id = inventory_manager->GetFirstOpenedWindowId();
      container = inventory_manager->GetWindow(container_id);
    }

    if (!container) {
      LOG_WARNING("Container closed during chest cleaning");
      return Status::Failure;
    }

    std::vector<short> slots_to_remove;
    std::vector<short> free_slots_inventory;
    std::vector<short> trash_inventory_slots;
    // Get slots to remove and free slots in inventory
    {
      std::lock_guard<std::mutex> inventory_manager_lock(
          inventory_manager->GetMutex());
      for (const auto& s : container->GetSlots()) {
        if (s.first < container->GetFirstPlayerInventorySlot() &&
            !s.second.IsEmptySlot() &&
#if PROTOCOL_VERSION < 340
            AssetsManager::getInstance()
                    .Items()
                    .at(s.second.GetBlockID())
                    .at(s.second.GetItemDamage())
                    ->GetName() != item_to_keep
#else
            AssetsManager::getInstance()
                    .Items()
                    .at(s.second.GetItemID())
                    ->GetName() != item_to_keep
#endif
        ) {
          slots_to_remove.push_back(s.first);
        } else if (s.first >= container->GetFirstPlayerInventorySlot() &&
                   s.second.IsEmptySlot()) {
          free_slots_inventory.push_back(s.first);
        }
      }
    }

    if (free_slots_inventory.size() == 0) {
      LOG_WARNING("Can't clean chest, no free slot in inventory to take items");
      CloseContainer(client, container_id);
      return Status::Failure;
    }

    // Go through as many slots as possible and transfer them in the inventory
    int index = 0;
    while (index < slots_to_remove.size() &&
           index < free_slots_inventory.size()) {
      if (SwapItemsInContainer(client, container_id, slots_to_remove[index],
                               free_slots_inventory[index]) ==
          Status::Failure) {
        LOG_WARNING(
            "Error trying to swap transfer items from chest to inventory");
        CloseContainer(client, container_id);
        return Status::Failure;
      }
      trash_inventory_slots.push_back(free_slots_inventory[index] -
                                      container->GetFirstPlayerInventorySlot() +
                                      9 /*Window::INVENTORY_STORAGE_START*/);
      index++;
    }

    CloseContainer(client, container_id);

    if (trash_inventory_slots.size() == 0) {
      return Status::Success;
    }

    has_wrong_items = index < slots_to_remove.size();

    // Now go to the cactus to destroy them
    if (GoTo(client, cactus_standing_pos, 0, 0) == Status::Failure) {
      LOG_WARNING("Error trying to go to the cactus standing position");
      return Status::Failure;
    }

    LookAt(client, Vector3<double>(0.5) + cactus_pos, false);

    for (int i = 0; i < trash_inventory_slots.size(); ++i) {
      if (DropItemsFromContainer(client, 0 /*Window::PLAYER_INVENTORY_INDEX*/,
                                 trash_inventory_slots[i]) == Status::Failure) {
        LOG_WARNING("Error trying to drop the items on the cactus");
        return Status::Failure;
      }
      for (int i = 0; i < 50; ++i) {
        client.Yield();
      }
    }
  }

  return Status::Success;
}

Status StoreDispenser(BehaviourClient& client) {
  Blackboard& b = client.GetBlackboard();
  std::shared_ptr<InventoryManager> inventory_manager =
      client.GetInventoryManager();

  if (OpenContainer(client,
                    b.Get<Position>("HarvestBot.output_shulker_position")) ==
      Status::Failure) {
    LOG_WARNING("Can't open output chest to store crafted dispenser");
    return Status::Failure;
  }
  for (int i = 0; i < 100; ++i) {
    client.Yield();
  }

  short container_id;
  std::shared_ptr<Window> container;
  {
    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    container_id = inventory_manager->GetFirstOpenedWindowId();
    container = inventory_manager->GetWindow(container_id);
  }

  if (!container) {
    LOG_WARNING("Container closed during dispenser storing");
    return Status::Failure;
  }

  short dst_slot = -1;
  short src_slot = -1;
  {
    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    const auto dispenser_id =
        AssetsManager::getInstance().GetItemID("minecraft:dispenser");
    for (const auto& s : container->GetSlots()) {
      if (dst_slot == -1 &&
          s.first < container->GetFirstPlayerInventorySlot() &&
          (s.second.IsEmptySlot() ||
           (
#if PROTOCOL_VERSION < 340
               s.second.GetBlockID() == dispenser_id.first &&
               s.second.GetItemDamage() == dispenser_id.second
#else
               s.second.GetItemID() == dispenser_id
#endif
               && s.second.GetItemCount() < AssetsManager::getInstance()
                                                    .Items()
                                                    .at(dispenser_id)
                                                    ->GetStackSize() -
                                                1))) {
        dst_slot = s.first;
      } else if (src_slot == -1 &&
                 s.first >= container->GetFirstPlayerInventorySlot() &&
#if PROTOCOL_VERSION < 340
                 s.second.GetBlockID() == dispenser_id.first &&
                 s.second.GetItemDamage() == dispenser_id.second
#else
                 s.second.GetItemID() == dispenser_id
#endif
      ) {
        src_slot = s.first;
      } else if (src_slot != -1 && dst_slot != -1) {
        break;
      }
    }
  }

  // No dispenser in inventory, nothing to do
  if (src_slot == -1) {
    CloseContainer(client);
    return Status::Success;
  }

  if (dst_slot == -1) {
    LOG_WARNING("Can't find a place for the dispenser in the chest");
    CloseContainer(client);
    return Status::Failure;
  }

  if (PutOneItemInContainerSlot(client, container_id, src_slot, dst_slot) ==
      Status::Failure) {
    LOG_WARNING("Error trying to transfer dispenser into chest");
    return Status::Failure;
  }

  CloseContainer(client);

  return Status::Success;
}

Status MineCobblestone(BehaviourClient& client) {
  std::shared_ptr<World> world = client.GetWorld();
  Blackboard& blackboard = client.GetBlackboard();

  const std::vector<Position>& stone_positions =
      blackboard.Get<std::vector<Position>>("HarvestBot.stone_positions");

  const Position* mining_pos = nullptr;
  {
    std::lock_guard<std::mutex> world_lock(world->GetMutex());
    for (int i = 0; i < stone_positions.size(); ++i) {
      const Block* block = world->GetBlock(stone_positions[i]);
      // Depending on the tick on which lava flows we could also get some cobble
      if (block &&
          (block->GetBlockstate()->GetName() == "minecraft:stone" ||
           block->GetBlockstate()->GetName() == "minecraft:cobblestone")) {
        mining_pos = stone_positions.data() + i;
        break;
      }
    }
  }

  // No stone, trigger the note block
  if (!mining_pos) {
    const Position& note_block_pos =
        blackboard.Get<Position>("HarvestBot.note_block_position");
    if (InteractWithBlock(client, note_block_pos, Direction::Down, true) ==
        Status::Failure) {
      LOG_WARNING("Error trying to activate the stone note block");
      return Status::Failure;
    }
    for (int i = 0; i < 50; ++i) {
      client.Yield();
    }
  }

  // Wait until we have one stone available (should be quick)
  auto start = std::chrono::steady_clock::now();
  while (!mining_pos) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count() >= 5000) {
      LOG_WARNING("Error waiting for stone block (Timeout).");
      return Status::Failure;
    }

    {
      std::lock_guard<std::mutex> world_lock(world->GetMutex());
      for (int i = 0; i < stone_positions.size(); ++i) {
        const Block* block = world->GetBlock(stone_positions[i]);
        // Depending on the tick on which lava flows we could also get some
        // cobble
        if (block &&
            (block->GetBlockstate()->GetName() == "minecraft:stone" ||
             block->GetBlockstate()->GetName() == "minecraft:cobblestone")) {
          mining_pos = stone_positions.data() + i;
          break;
        }
      }
    }

    if (!mining_pos) {
      client.Yield();
    }
  }

  if (Dig(client, *mining_pos, true, Direction::North) == Status::Failure) {
    LOG_WARNING("Error digging stone.");
    return Status::Failure;
  }

  const Position& stone_shulker_position =
      blackboard.Get<Position>("HarvestBot.stone_shulker_position");
  if (OpenContainer(client, stone_shulker_position) == Status::Failure) {
    LOG_WARNING("Error trying to open stone shulker.");
    return Status::Failure;
  }

  std::shared_ptr<InventoryManager> inventory_manager =
      client.GetInventoryManager();
  short container_id;
  std::shared_ptr<Window> shulker_container;
  {
    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    container_id = inventory_manager->GetFirstOpenedWindowId();
    shulker_container = inventory_manager->GetWindow(container_id);
  }

  if (!shulker_container) {
    LOG_WARNING("Container closed during stone gathering");
    return Status::Failure;
  }

  short src_slot = -1;
  short dst_slot = -1;
  start = std::chrono::steady_clock::now();
  // Wait for a cobblestone stack to be in the shulker
  while (src_slot == -1) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start)
            .count() >= 5000) {
      LOG_WARNING("Error waiting for stone item in shulker (Timeout).");
      CloseContainer(client, container_id);
      return Status::Failure;
    }

    {
      std::lock_guard<std::mutex> lock_inventory_manager(
          inventory_manager->GetMutex());
      dst_slot = -1;
      int quantity = 0;
      for (const auto& s : shulker_container->GetSlots()) {
        if (src_slot == -1 &&
            s.first < shulker_container->GetFirstPlayerInventorySlot()) {
#if PROTOCOL_VERSION < 340
          if (!s.second.IsEmptySlot() &&
              AssetsManager::getInstance()
                      .Items()
                      .at(s.second.GetBlockID())
                      .at(s.second.GetItemDamage())
                      ->GetName() == "minecraft:cobblestone")
#else
          if (!s.second.IsEmptySlot() &&
              AssetsManager::getInstance()
                      .Items()
                      .at(s.second.GetItemID())
                      ->GetName() == "minecraft:cobblestone")
#endif
          {
            src_slot = s.first;
            quantity = s.second.GetItemCount();
            if (dst_slot != -1) {
              break;
            }
          }
        }

        if (src_slot != -1 &&
            s.first >= shulker_container->GetFirstPlayerInventorySlot()) {
          if (s.second.IsEmptySlot() ||
              (s.second.GetItemCount() < 64 - quantity &&
#if PROTOCOL_VERSION < 340
               AssetsManager::getInstance()
                       .Items()
                       .at(s.second.GetBlockID())
                       .at(s.second.GetItemDamage())
                       ->GetName() == "minecraft:cobblestone"
#else
               AssetsManager::getInstance()
                       .Items()
                       .at(s.second.GetItemID())
                       ->GetName() == "minecraft:cobblestone"
#endif
               )) {
            dst_slot = s.first;
            break;
          }
        }
      }
    }

    if (src_slot != -1 && dst_slot == -1) {
      LOG_WARNING("Error trying to take stone from shulker, no slot available");
      CloseContainer(client, container_id);
      return Status::Failure;
    }

    if (src_slot != -1 && dst_slot != -1) {
      break;
    }

    client.Yield();
  }

  if (SwapItemsInContainer(client, container_id, src_slot, dst_slot) ==
      Status::Failure) {
    LOG_WARNING("Error getting the stone from the shulker");
    CloseContainer(client, container_id);
    return Status::Failure;
  }

  CloseContainer(client, container_id);

  return Status::Success;
}

Status TakeFromChest(BehaviourClient& client,
                     const std::string& item_name,
                     const int N) {
  std::shared_ptr<InventoryManager> inventory_manager =
      client.GetInventoryManager();

  short container_id;
  std::shared_ptr<Window> container;
  {
    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    container_id = inventory_manager->GetFirstOpenedWindowId();
    container = inventory_manager->GetWindow(container_id);
  }

  if (!container) {
    LOG_WARNING("Container closed during in chest item taking");
    return Status::Failure;
  }

  std::vector<short> available_slots;
  std::vector<short> to_take_slots;
  int num_in_inventory = 0;
  {
    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    for (const auto& s : container->GetSlots()) {
      if (!s.second.IsEmptySlot() &&
#if PROTOCOL_VERSION < 340
          AssetsManager::getInstance()
                  .Items()
                  .at(s.second.GetBlockID())
                  .at(s.second.GetItemDamage())
                  ->GetName() == item_name
#else
          AssetsManager::getInstance()
                  .Items()
                  .at(s.second.GetItemID())
                  ->GetName() == item_name
#endif
      ) {
        if (s.first < container->GetFirstPlayerInventorySlot()) {
          to_take_slots.push_back(s.first);
        } else {
          num_in_inventory += s.second.GetItemCount();
        }
      } else if (s.first >= container->GetFirstPlayerInventorySlot() &&
                 s.second.IsEmptySlot()) {
        available_slots.push_back(s.first);
      }
    }
  }

  int index = 0;
  while (num_in_inventory < N && index < to_take_slots.size() &&
         index < available_slots.size()) {
    if (SwapItemsInContainer(client, container_id, to_take_slots[index],
                             available_slots[index]) == Status::Failure) {
      LOG_WARNING("Error trying to take an item from a chest");
      return Status::Failure;
    }

    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    num_in_inventory +=
        container->GetSlot(available_slots[index]).GetItemCount();

    index += 1;
  }

  return num_in_inventory >= N ? Status::Success : Status::Failure;
}

Status CollectCropsAndReplant(BehaviourClient& client,
                              const std::string& blocks_pos_blackboard,
                              const std::string& item_name) {
  std::shared_ptr<World> world = client.GetWorld();
  Blackboard& blackboard = client.GetBlackboard();

  const std::vector<Position>& positions =
      blackboard.Get<std::vector<Position>>(blocks_pos_blackboard);

  for (int i = 0; i < positions.size(); ++i) {
    bool ready_to_harvest = false;
    {
      std::lock_guard<std::mutex> lock_world(world->GetMutex());
      const Block* block = world->GetBlock(positions[i]);
      ready_to_harvest = !block->GetBlockstate()->IsAir() &&
                         block->GetBlockstate()->GetVariableValue("age") == "7";
    }

    if (ready_to_harvest) {
      if (Dig(client, positions[i], true) == Status::Failure) {
        LOG_WARNING("Error trying to break a crop");
        return Status::Failure;
      }
      for (int i = 0; i < 50; ++i) {
        client.Yield();
      }
    }
  }

  std::shared_ptr<EntityManager> entity_manager = client.GetEntityManager();

  std::map<int, Vector3<double>> entity_positions;

  bool moving_items = true;
  while (moving_items) {
    moving_items = false;
    entity_positions.clear();
    {
      std::lock_guard<std::mutex> entity_manager_lock(
          entity_manager->GetMutex());
      for (const auto& e : entity_manager->GetEntities()) {
        if (e.second->GetType() == EntityType::ItemEntity) {
          std::shared_ptr<ItemEntity> item =
              std::static_pointer_cast<ItemEntity>(e.second);
#if PROTOCOL_VERSION < 340
          if (AssetsManager::getInstance()
                  .Items()
                  .at(item->GetDataItem().GetBlockID())
                  .at(item->GetDataItem().GetItemDamage())
                  ->GetName() == item_name)
#else
          if (AssetsManager::getInstance()
                  .Items()
                  .at(item->GetDataItem().GetItemID())
                  ->GetName() == item_name)
#endif
          {
            entity_positions[e.first] = e.second->GetPosition();
            moving_items = e.second->GetSpeed().SqrNorm() > 0.0001;
            if (moving_items) {
              break;
            }
          }
        }
      }
    }
    client.Yield();
  }

  for (const auto& e : entity_positions) {
    if (GoTo(client, e.second, 2) == Status::Failure) {
      LOG_WARNING(
          "Error trying to pick up a crop item (can't get close enough to "
          << e.second << ")");
      return Status::Failure;
    }

    auto start = std::chrono::steady_clock::now();
    while (true) {
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - start)
              .count() >= 5000) {
        LOG_WARNING("Error waiting for crop item pick-up (Timeout).");
        return Status::Failure;
      }

      {
        std::lock_guard<std::mutex> entity_manager_lock(
            entity_manager->GetMutex());
        if (entity_manager->GetEntity(e.first) == nullptr) {
          break;
        }
      }

      client.Yield();
    }
  }

  for (int i = positions.size() - 1; i >= 0; --i) {
    bool to_replant = false;
    {
      std::lock_guard<std::mutex> lock_world(world->GetMutex());
      const Block* block = world->GetBlock(positions[i]);
      to_replant = block->GetBlockstate()->IsAir();
    }

    if (to_replant) {
      if (PlaceBlock(client, item_name, positions[i]) == Status::Failure) {
        LOG_WARNING("Error trying to replant a crop");
        return Status::Failure;
      }

      for (int i = 0; i < 50; ++i) {
        client.Yield();
      }
    }
  }

  return Status::Success;
}

Status DestroyItems(BehaviourClient& client, const std::string& item_name) {
  Blackboard& blackboard = client.GetBlackboard();
  std::shared_ptr<InventoryManager> inventory_manager =
      client.GetInventoryManager();
  std::shared_ptr<Window> inventory = inventory_manager->GetPlayerInventory();

  const Position& cactus_standing_pos =
      blackboard.Get<Position>("HarvestBot.cactus_standing_position");
  const Position& cactus_pos =
      blackboard.Get<Position>("HarvestBot.cactus_position");

  std::vector<short> slots_to_remove;
  {
    std::lock_guard<std::mutex> lock_inventory_manager(
        inventory_manager->GetMutex());
    for (const auto& s : inventory->GetSlots()) {
      if (s.first >= inventory->GetFirstPlayerInventorySlot() &&
          !s.second.IsEmptySlot() &&
#if PROTOCOL_VERSION < 340
          AssetsManager::getInstance()
                  .Items()
                  .at(s.second.GetBlockID())
                  .at(s.second.GetItemDamage())
                  ->GetName() == item_name
#else
          AssetsManager::getInstance()
                  .Items()
                  .at(s.second.GetItemID())
                  ->GetName() == item_name
#endif
      ) {
        slots_to_remove.push_back(s.first);
      }
    }
  }

  if (slots_to_remove.size() == 0) {
    return Status::Success;
  }

  if (GoTo(client, cactus_standing_pos, 0, 0) == Status::Failure) {
    LOG_WARNING("Error trying to go to the cactus standing position");
    return Status::Failure;
  }

  LookAt(client, Vector3<double>(0.5) + cactus_pos, false);

  for (int i = 0; i < slots_to_remove.size(); ++i) {
    if (DropItemsFromContainer(client, 0 /*Window::PLAYER_INVENTORY_INDEX*/,
                               slots_to_remove[i]) == Status::Failure) {
      LOG_WARNING("Error trying to drop the items on the cactus");
      return Status::Failure;
    }
  }
  return Status::Success;
}
