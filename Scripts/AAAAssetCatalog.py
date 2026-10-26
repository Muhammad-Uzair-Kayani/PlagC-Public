"""Stable output names for the CC0 industrial asset set."""

ASSETS = [
    ("old_military_crate", "Assets/Cooked/TPS/AAA/Crate.plasset"),
    ("portable_generator", "Assets/Cooked/TPS/AAA/Generator.plasset"),
    ("security_camera_01", "Assets/Cooked/TPS/AAA/SecurityCamera.plasset"),
    ("security_camera_02", "Assets/Cooked/TPS/AAA/SecurityCamera02.plasset"),
    ("modular_factory_facade", "Assets/Cooked/TPS/AAA/FactoryFacade.plasset"),
    ("modular_industrial_pipes_01", "Assets/Cooked/TPS/AAA/IndustrialPipes.plasset"),
    ("modular_pipes", "Assets/Cooked/TPS/AAA/ModularPipes.plasset"),
    ("steel_frame_shelves_01", "Assets/Cooked/TPS/AAA/SteelShelves.plasset"),
    ("exterior_aircon_unit", "Assets/Cooked/TPS/AAA/ExteriorAircon.plasset"),
    ("utility_box_01", "Assets/Cooked/TPS/AAA/UtilityBox.plasset"),
    ("barrel_03", "Assets/Cooked/TPS/AAA/Barrel.plasset"),
    ("plastic_crate_02", "Assets/Cooked/TPS/AAA/PlasticCrate.plasset"),
    ("modular_airduct_rectangular_01", "Assets/Cooked/TPS/AAA/Airduct.plasset"),
    ("modular_airduct_circular_01", "Assets/Cooked/TPS/AAA/CircularAirduct.plasset"),
    ("modular_electric_cables", "Assets/Cooked/TPS/AAA/ElectricCables.plasset"),
    ("modular_chainlink_fence", "Assets/Cooked/TPS/AAA/ChainlinkFence.plasset"),
    ("concrete_road_barrier", "Assets/Cooked/TPS/AAA/RoadBarrier.plasset"),
    ("metal_jerrycan", "Assets/Cooked/TPS/AAA/Jerrycan.plasset"),
    ("industrial_wall_lamp", "Assets/Cooked/TPS/AAA/WallLamp.plasset"),
    ("industrial_caged_sconce", "Assets/Cooked/TPS/AAA/CagedSconce.plasset"),
    ("mounted_fluorescent_lights", "Assets/Cooked/TPS/AAA/FluorescentLights.plasset"),
    ("fire_alarm", "Assets/Cooked/TPS/AAA/FireAlarm.plasset"),
    ("metal_toolbox", "Assets/Cooked/TPS/AAA/Toolbox.plasset"),
    ("hand_truck", "Assets/Cooked/TPS/AAA/HandTruck.plasset"),
]

# Explicit runtime parts/variants; source kits remain unchanged on disk.
NODE_RECIPES = {
    "exterior_aircon_unit": {"node": "exterior_aircon_unit"},
    "industrial_caged_sconce": {"node": "industrial_caged_sconce_a"},
    "modular_airduct_circular_01": {"node": "modular_airduct_circular_single"},
    "modular_airduct_rectangular_01": {
        "node": "modular_airduct_rectangular_01_single_01"
    },
    "modular_chainlink_fence": {"node": "modular_chainlink_fence_post"},
    "modular_electric_cables": {"node": "cable_straight_long"},
    "modular_factory_facade": {"node": "wall_standard_standard_01"},
    "modular_industrial_pipes_01": {"node": "modular_industrial_pipes_01_pipe01"},
    "modular_pipes": {"node": "pipe_200cm_metal"},
    "mounted_fluorescent_lights": {"node": "mounted_fluorescent_lights_a"},
    "old_military_crate": {
        "nodes": [
            "old_military_crate_lid_b",
            "old_military_crate_loop_b",
            "old_military_crate_latch_b",
            "old_military_crate_cloth_b",
            "old_military_crate_b",
        ]
    },
}
