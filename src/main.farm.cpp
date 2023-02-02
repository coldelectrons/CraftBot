#include <iostream>
#include <string>

#include "botcraft/AI/SimpleBehaviourClient.hpp"
#include "botcraft/AI/Tasks/AllTasks.hpp"
#include "botcraft/Utilities/Logger.hpp"

#include "DispenserFarmTasks.hpp"


void ShowHelp(const char* argv0)
{
    std::cout << "Usage: " << argv0 << " <options>\n"
        << "Options:\n"
        << "\t-h, --help\tShow this help message\n"
        << "\t--address\tAddress of the server you want to connect to, default: 127.0.0.1:25565\n"
        << "\t--login\t\tPlayer name in offline mode, empty for Microsoft account, default: BCDispenserGuy\n"
        << std::endl;
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateTree();

int main(int argc, char* argv[])
{
    try
    {
        // Init logging, log everything >= Info, only to console, no file
        Botcraft::Logger::GetInstance().SetLogLevel(Botcraft::LogLevel::Info);
        Botcraft::Logger::GetInstance().SetFilename("");
        // Add a name to this thread for logging
        Botcraft::Logger::GetInstance().RegisterThread("main");

        std::string address = "127.0.0.1:25565";
        std::string login = "BCDispenserGuy";

        if (argc == 1)
        {
            LOG_WARNING("No command arguments. Using default options.");
            ShowHelp(argv[0]);
        }

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help")
            {
                ShowHelp(argv[0]);
                return 0;
            }
            else if (arg == "--address")
            {
                if (i + 1 < argc)
                {
                    address = argv[++i];
                }
                else
                {
                    LOG_FATAL("--address requires an argument");
                    return 1;
                }
            }
            else if (arg == "--login")
            {
                if (i + 1 < argc)
                {
                    login = argv[++i];
                }
                else
                {
                    LOG_FATAL("--login requires an argument");
                    return 1;
                }
            }
        }

        auto dispenser_farm_tree = CreateTree();

        Botcraft::SimpleBehaviourClient client(true);
        client.SetAutoRespawn(true);

        LOG_INFO("Starting connection process");
        client.Connect(address, login);

        // Wait 10 seconds then start executing the tree
        Botcraft::SleepFor(std::chrono::seconds(10));
        client.SetBehaviourTree(dispenser_farm_tree);

        client.RunBehaviourUntilClosed();

        client.Disconnect();

        return 0;
    }
    catch (std::exception& e)
    {
        LOG_FATAL("Exception: " << e.what());
        return 1;
    }
    catch (...)
    {
        LOG_FATAL("Unknown exception");
        return 2;
    }
}


std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateBuyTree(const std::string& item_name, const std::string& blackboard_entity_location)
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        // Buy an item if we don't have one already
        .selector()
            .leaf(Botcraft::HasItemInInventory, item_name, 1)
            .sequence()
                .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.buying_standing_position", "GoTo.goal")
                .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 0)
                .leaf(Botcraft::GoToBlackboard)
                .leaf(CopyRandomFromVectorBlackboardData<int>, blackboard_entity_location, "InteractEntity.entity_id")
                .leaf(Botcraft::SetBlackboardData<bool>, "InteractEntity.swing", true)
                .leaf(Botcraft::InteractEntityBlackboard)
                .repeater(100)
                    .leaf(Botcraft::Yield)
                .end()
                .selector()
                    // If trading fail, we still want to close the container
                    .leaf(Botcraft::TradeName, item_name, true, -1)
                    .leaf([](Botcraft::SimpleBehaviourClient& c) { Botcraft::CloseContainer(c, -1); return Botcraft::Status::Failure;})
                .end()
                .leaf(Botcraft::CloseContainer, -1)
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateRottenFleshEmeraldTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
            .selector()
                // If we don't have rotten flesh, take some in the chest
                .leaf(Botcraft::HasItemInInventory, "minecraft:rotten_flesh", 64)
                .sequence()
                    .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.buying_standing_position", "GoTo.goal")
                    .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 0)
                    .leaf(Botcraft::GoToBlackboard)
                    .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.rotten_flesh_shulker_position", "OpenContainer.pos")
                    .leaf(Botcraft::OpenContainerBlackboard)
                    .repeater(100)
                        .leaf(Botcraft::Yield)
                    .end()
                    .succeeder()
                        .leaf(TakeFromChest, "minecraft:rotten_flesh", 64)
                    .end()
                    .leaf(Botcraft::CloseContainer, -1)
                .end()
            .end()
            // Trade some rotten flesh
            .repeater(0)
                .inverter()
                    .sequence()
                        .leaf(Botcraft::HasItemInInventory, "minecraft:rotten_flesh", 32)
                        .leaf(CopyRandomFromVectorBlackboardData<int>, "DispenserFarmBot.cleric_id", "InteractEntity.entity_id")
                        .leaf(Botcraft::SetBlackboardData<bool>, "InteractEntity.swing", true)
                        .leaf(Botcraft::InteractEntityBlackboard)
                        .selector()
                            // If trading fail, we still want to close the container
                            .leaf(Botcraft::TradeName, "minecraft:rotten_flesh", false, -1)
                            .leaf([](Botcraft::SimpleBehaviourClient& c) { Botcraft::CloseContainer(c, -1); return Botcraft::Status::Failure;})
                        .end()
                        .leaf(Botcraft::CloseContainer, -1)
                        .repeater(50)
                            .leaf(Botcraft::Yield)
                        .end()              
                    .end()
                .end()
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateBonesEmeraldTree()
{
    std::array<std::array<std::string, 3>, 3> recipe_bone_meal, recipe_white_dye;
    recipe_bone_meal[0][0] = "minecraft:bone";
    recipe_white_dye[0][0] = "minecraft:bone_meal";

    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
            .selector()
                // If we don't have white dye, take some bones in the chest
                .leaf(Botcraft::HasItemInInventory, "minecraft:white_dye", 64)
                .sequence()
                    .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.buying_standing_position", "GoTo.goal")
                    .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 0)
                    .leaf(Botcraft::GoToBlackboard)
                    .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.bones_shulker_position", "OpenContainer.pos")
                    .leaf(Botcraft::OpenContainerBlackboard)
                    .repeater(100)
                        .leaf(Botcraft::Yield)
                    .end()
                    .succeeder()
                        .leaf(TakeFromChest, "minecraft:bone", 32)
                    .end()
                    .leaf(Botcraft::CloseContainer, -1)
                .end()
            .end()
            // While we have at least one bone
            // Craft as many bone meal as possible
            .repeater(0)
                .inverter()
                    .sequence()
                        .leaf(Botcraft::HasItemInInventory, "minecraft:bone", 1)
                        .leaf(Botcraft::CraftNamed, recipe_bone_meal, true)
                    .end()
                .end()
            .end()
            // While we have at least one bone meal
            // Craft as many white dye as possible
            .repeater(0)
                .inverter()
                    .sequence()
                        .leaf(Botcraft::HasItemInInventory, "minecraft:bone_meal", 1)
                        .leaf(Botcraft::CraftNamed, recipe_white_dye, true)
                    .end()
                .end()
            .end()
            // While we have some white dye, sell them
            .repeater(0)
                .inverter()
                    .sequence()
                        .leaf(Botcraft::HasItemInInventory, "minecraft:white_dye", 12)
                        .leaf(CopyRandomFromVectorBlackboardData<int>, "DispenserFarmBot.shepperd_id", "InteractEntity.entity_id")
                        .leaf(Botcraft::SetBlackboardData<bool>, "InteractEntity.swing", true)
                        .leaf(Botcraft::InteractEntityBlackboard)
                        .selector()
                            // If trading fail, we still want to close the container
                            .leaf(Botcraft::TradeName, "minecraft:white_dye", false, -1)
                            .leaf([](Botcraft::SimpleBehaviourClient& c) { Botcraft::CloseContainer(c, -1); return Botcraft::Status::Failure;})
                        .end()
                        .leaf(Botcraft::CloseContainer, -1)
                        .repeater(50)
                            .leaf(Botcraft::Yield)
                        .end()              
                    .end()
                .end()
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateCropEmeraldTree(const std::string& item_name, const std::string& blackboard_crops_location)
{
    int min_item_to_sell = item_name == "minecraft:carrot" ? 22 : 26;
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
        // Collect crops if we don't have some
            .selector()
                .leaf(Botcraft::HasItemInInventory, item_name, 64)
                .leaf(CollectCropsAndReplant, blackboard_crops_location, item_name)
            .end()
            .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.buying_standing_position", "GoTo.goal")
            .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 0)
            .leaf(Botcraft::GoToBlackboard)
        // Sell crops
            .repeater(0)
                .inverter()
                    .sequence()
                        .leaf(Botcraft::HasItemInInventory, item_name, min_item_to_sell)
                        .leaf(CopyRandomFromVectorBlackboardData<int>, "DispenserFarmBot.farmer_id", "InteractEntity.entity_id")
                        .leaf(Botcraft::SetBlackboardData<bool>, "InteractEntity.swing", true)
                        .leaf(Botcraft::InteractEntityBlackboard)
                        .repeater(100)
                            .leaf(Botcraft::Yield)
                        .end()
                        .selector()
                            // If trading fail, we still want to close the container
                            .leaf(Botcraft::TradeName, item_name, false, -1)
                            .leaf([](Botcraft::SimpleBehaviourClient& c) { Botcraft::CloseContainer(c, -1); return Botcraft::Status::Failure;})
                        .end()
                        .leaf(Botcraft::CloseContainer, -1)                
                    .end()
                .end()
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateEatTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .selector()
            // If hungry
            .inverter()
                .leaf(Botcraft::IsHungry)
            .end()
            // Go buy some food, then eat
            .sequence()
                .leaf(Botcraft::HasItemInInventory, "minecraft:emerald", 3)
                .tree(CreateBuyTree("minecraft:golden_carrot", "DispenserFarmBot.farmer_id"))
                .leaf(Botcraft::Eat, "minecraft:golden_carrot", true)
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateCollectCobblestoneTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .selector()
            // If we already have engouh, don't enter the loop
            .leaf(Botcraft::HasItemInInventory, "minecraft:cobblestone", 7)
            // Repeat 10 times (7 + 3 in case something goes wrong)
            .repeater(10)
                .selector()
                    // If already 7 in inventory, no need to get more
                    .leaf(Botcraft::HasItemInInventory, "minecraft:cobblestone", 7)
                    .sequence()
                        // Get a pickaxe if don't have one already
                        .selector()
                            .leaf(Botcraft::SetItemInHand, "minecraft:stone_pickaxe", Botcraft::Hand::Right)
                            .sequence()
                                .leaf(Botcraft::HasItemInInventory, "minecraft:emerald", 1)
                                .tree(CreateBuyTree("minecraft:stone_pickaxe", "DispenserFarmBot.toolsmith_id"))
                                .leaf(Botcraft::SetItemInHand, "minecraft:stone_pickaxe", Botcraft::Hand::Right)
                            .end()
                        .end()
                        // Go to the mining position
                        .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.stone_standing_position", "GoTo.goal")
                        .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 0)
                        .leaf(Botcraft::GoToBlackboard)
                        // Get cobblestone
                        .leaf(MineCobblestone)
                    .end()
                .end()
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateCraftDispenserTree()
{
    std::array<std::array<std::string, 3>, 3> dispenser_recipe;
    dispenser_recipe[0][0] = "minecraft:cobblestone";
    dispenser_recipe[0][1] = "minecraft:cobblestone";
    dispenser_recipe[0][2] = "minecraft:cobblestone";
    dispenser_recipe[1][0] = "minecraft:cobblestone";
    dispenser_recipe[1][1] = "minecraft:bow";
    dispenser_recipe[1][2] = "minecraft:cobblestone";
    dispenser_recipe[2][0] = "minecraft:cobblestone";
    dispenser_recipe[2][1] = "minecraft:redstone";
    dispenser_recipe[2][2] = "minecraft:cobblestone";

    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
            .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.buying_standing_position", "GoTo.goal")
            .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 0)
            .leaf(Botcraft::GoToBlackboard)
            .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.crafting_table_position", "OpenContainer.pos")
            .leaf(Botcraft::OpenContainerBlackboard)
            .repeater(100)
                .leaf(Botcraft::Yield)
            .end()
            .leaf(Botcraft::CraftNamed, dispenser_recipe, false)
            .leaf(Botcraft::CloseContainer, -1)
            .selector()
                .leaf(StoreDispenser)
        // Stop all behaviour if output chest is potentially full
                .sequence()
                    .leaf(Botcraft::Say, "Error trying to put dispenser in output chest, stopping behaviour...")
                    .leaf([](Botcraft::SimpleBehaviourClient& c) { c.SetBehaviourTree(nullptr); return Botcraft::Status::Success; })
                    .leaf(Botcraft::Yield)
                .end()
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateCleanStorageTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
            .leaf(CleanChest, "DispenserFarmBot.bones_shulker_position", "minecraft:bone")
            .leaf(CleanChest, "DispenserFarmBot.rotten_flesh_shulker_position", "minecraft:rotten_flesh")
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateDespawnFrostWalkerTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .selector()
            .inverter()
                .leaf(CheckFrostWalkerMob)
            .end()
            .sequence()
                .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.despawn_position", "GoTo.goal")
                .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 1)
                .leaf(Botcraft::GoToBlackboard)
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateSleepTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .selector()
            // If it's night
            .inverter()
                .leaf(Botcraft::IsNightTime)
            .end()
            .sequence()
                // Go to the bed
                .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.bed_position", "GoTo.goal")
                .leaf(Botcraft::SetBlackboardData<int>, "GoTo.dist_tolerance", 2)
                .leaf(Botcraft::GoToBlackboard)
                // Right click the bed every second until it's day time
                .repeater(0)
                    .sequence()
                        .leaf(Botcraft::CopyBlackboardData, "DispenserFarmBot.bed_position", "InteractWithBlock.pos")
                        .leaf(Botcraft::SetBlackboardData<bool>, "InteractWithBlock.animation", true)
                        .leaf(Botcraft::InteractWithBlockBlackboard)
                        // Wait ~1s
                        .repeater(100)
                            .leaf(Botcraft::Yield)
                        .end()
                        .inverter()
                            .leaf(Botcraft::IsNightTime)
                        .end()
                    .end()
                .end()
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateCollectEmeraldsTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
            .succeeder()
                .tree(CreateBonesEmeraldTree())
            .end()
            .succeeder()
                .tree(CreateRottenFleshEmeraldTree())
            .end()
            .succeeder()
                .tree(CreateCropEmeraldTree("minecraft:potato", "DispenserFarmBot.potato_positions"))
            .end()
            .succeeder()
                .tree(CreateCropEmeraldTree("minecraft:carrot", "DispenserFarmBot.carrot_positions"))
            .end()
        .end()
        .build();
}

std::shared_ptr<Botcraft::BehaviourTree<Botcraft::SimpleBehaviourClient>> CreateTree()
{
    return Botcraft::Builder<Botcraft::SimpleBehaviourClient>()
        .sequence()
            .selector()
                .leaf(Botcraft::CheckBlackboardBoolData, "DispenserFarmBot.initialized")
                .sequence()
                    .leaf(InitializeBlocks, 50)
                    .leaf(StartSpawners)
                .end()
            .end()
            .tree(CreateCollectEmeraldsTree())
            .tree(CreateCleanStorageTree())
            .leaf(Botcraft::SortInventory)
            .leaf(DestroyItems, "minecraft:poisonous_potato")
            .tree(CreateDespawnFrostWalkerTree())
            .tree(CreateSleepTree())
            .tree(CreateEatTree())
            .leaf(Botcraft::HasItemInInventory, "minecraft:emerald", 2)
            .tree(CreateBuyTree("minecraft:bow", "DispenserFarmBot.fletcher_id"))
            .leaf(Botcraft::HasItemInInventory, "minecraft:emerald", 1)
            .tree(CreateBuyTree("minecraft:redstone", "DispenserFarmBot.cleric_id"))
            .tree(CreateCollectCobblestoneTree())
            .tree(CreateCraftDispenserTree())
        .end()
        .build();
}
