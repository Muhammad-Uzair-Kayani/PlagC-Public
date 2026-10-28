#include "pgcpch.h"
#include "CityStreetWorld.h"
#include <PlagC/Animation/Animation.h>
#include <PlagC/Animation/AnimationRetarget.h>
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Input/Input.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace PlagCGame
{
namespace
{
constexpr float Rad = 3.14159265358979323846f / 180.0f;
constexpr float PlayerVisualScale = 100.0f;
constexpr float TerrainHalfExtent = 29.0f;
constexpr PlagC::Vec3 InitialPlayerPosition{0.0f, 0.0f, 0.0f};
constexpr std::string_view UpperBodyRoot = "Bip001 Spine1_012";

float LengthXZ(PlagC::Vec3 v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

float TerrainHeight(float x, float z)
{
    const float r = std::max(std::abs(x), std::abs(z));
    if (r <= 12.0f)
        return 0.0f;

    float t = std::clamp((r - 12.0f) / 12.0f, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    const float wave = 0.30f * std::sin(x * 0.17f) +
                       0.22f * std::cos(z * 0.21f) +
                       0.12f * std::sin((x + z) * 0.11f);
    return t * wave;
}

unsigned Clip(const PlagC::ModelAsset &asset, std::string_view name)
{
    auto it = std::find_if(asset.animations.begin(), asset.animations.end(),
                           [&](const auto &clip) { return clip.name == name; });
    if (it == asset.animations.end())
        throw std::runtime_error("required animation clip missing: " + std::string(name));
    return unsigned(it - asset.animations.begin());
}

std::unordered_map<std::string, std::string> JokerToUAL()
{
    std::unordered_map<std::string, std::string> map = {
        {"Bip001_01", "root"},
        {"Bip001 Pelvis_02", "pelvis"},
        {"Bip001 Spine_03", "spine_01"},
        {"Bip001 Spine1_012", "spine_02"},
        {"Bip001 Neck_013", "neck_01"},
        {"Bip001 Head_00", "Head"},
        {"Bip001 L Clavicle_015", "clavicle_l"},
        {"Bip001 L UpperArm_016", "upperarm_l"},
        {"Bip001 L Forearm_017", "lowerarm_l"},
        {"Bip001 L Hand_018", "hand_l"},
        {"Bip001 R Clavicle_035", "clavicle_r"},
        {"Bip001 R UpperArm_036", "upperarm_r"},
        {"Bip001 R Forearm_037", "lowerarm_r"},
        {"Bip001 R Hand_038", "hand_r"},
        {"Bip001 L Thigh_04", "thigh_l"},
        {"Bip001 L Calf_05", "calf_l"},
        {"Bip001 L Foot_06", "foot_l"},
        {"Bip001 L Toe0_07", "ball_l"},
        {"Bip001 R Thigh_08", "thigh_r"},
        {"Bip001 R Calf_09", "calf_r"},
        {"Bip001 R Foot_010", "foot_r"},
        {"Bip001 R Toe0_011", "ball_r"},
    };

    struct FingerMap
    {
        const char *biped;
        const char *ual;
    } fingers[] = {
        {"Finger0", "thumb"}, {"Finger1", "index"}, {"Finger2", "middle"},
        {"Finger3", "ring"}, {"Finger4", "pinky"},
    };

    for (const char side : {'L', 'R'})
    {
        const char *ualSide = side == 'L' ? "l" : "r";
        const unsigned base = side == 'L' ? 19u : 39u;
        for (unsigned finger = 0; finger < 5; ++finger)
            for (unsigned segment = 0; segment < 3; ++segment)
            {
                std::string target = "Bip001 ";
                target += side;
                target += ' ';
                target += fingers[finger].biped;
                if (segment == 1)
                    target += '1';
                else if (segment == 2)
                    target += '2';
                target += '_';
                target += std::to_string(base + finger * 3 + segment);

                std::string source = fingers[finger].ual;
                source += "_0" + std::to_string(segment + 1) + "_" + ualSide;
                map.emplace(std::move(target), std::move(source));
            }
    }
    return map;
}

PlagC::AnimationStateMachine LocomotionGraph(const PlagC::ModelAsset &asset)
{
    const unsigned idle = Clip(asset, "Idle_Loop");
    const unsigned walk = Clip(asset, "Walk_Loop");
    const unsigned jog = Clip(asset, "Jog_Fwd_Loop");
    const unsigned sprint = Clip(asset, "Sprint_Loop");

    using PT = PlagC::AnimationParameterType;
    using C = PlagC::AnimationCompare;

    PlagC::AnimationStateMachine graph;
    graph.parameters = {{"Speed", PT::Float, 0.0f}};
    graph.states = {{"Idle", idle, true, 1.0f},
                    {"Walk", walk, true, 1.0f},
                    {"Jog", jog, true, 1.0f},
                    {"Sprint", sprint, true, 1.0f}};
    graph.entry = 0;

    auto transition = [&](unsigned from, unsigned to, float fade,
                          std::initializer_list<PlagC::AnimationCondition> conditions) {
        PlagC::AnimationTransition t;
        t.from = from;
        t.to = to;
        t.crossfade = fade;
        t.syncNormalizedPhase = from != 0 && to != 0;
        t.conditions.assign(conditions.begin(), conditions.end());
        graph.transitions.push_back(std::move(t));
    };

    // Declaration order is priority. Direct jumps avoid taking several frames to
    // traverse Walk->Jog->Sprint when the desired speed changes abruptly.
    transition(0, 3, .20f, {{"Speed", C::Greater, 4.8f}});
    transition(0, 2, .18f, {{"Speed", C::Greater, 2.2f}});
    transition(0, 1, .16f, {{"Speed", C::Greater, .12f}});

    transition(1, 3, .18f, {{"Speed", C::Greater, 4.8f}});
    transition(1, 2, .16f, {{"Speed", C::Greater, 2.2f}});
    transition(1, 0, .18f, {{"Speed", C::Less, .12f}});

    transition(2, 3, .14f, {{"Speed", C::Greater, 4.8f}});
    transition(2, 1, .16f, {{"Speed", C::Greater, .12f}, {"Speed", C::Less, 2.2f}});
    transition(2, 0, .18f, {{"Speed", C::Less, .12f}});

    transition(3, 2, .16f, {{"Speed", C::Greater, 2.2f}, {"Speed", C::Less, 4.8f}});
    transition(3, 1, .18f, {{"Speed", C::Greater, .12f}, {"Speed", C::Less, 2.2f}});
    transition(3, 0, .20f, {{"Speed", C::Less, .12f}});

    return graph;
}

PlagC::Entity ModelEntity(PlagC::World &world, std::string name,
                          PlagC::AssetHandle<PlagC::ModelAsset> asset,
                          const PlagC::Transform &transform)
{
    if (!asset)
        throw std::runtime_error("cannot create model entity from an empty asset handle: " + name);
    auto entity = world.Create(std::move(name));
    world.SetTransform(entity, transform);
    world.SetModel(entity, {std::move(asset), true});
    return entity;
}
} // namespace

void CityStreetWorld::Start(PlagC::Engine &engine)
{
    auto terrain = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Terrain/OpenTerrain/OpenTerrain.plasset");
    auto joker = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joker/Joker.plasset");
    auto ual = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Animations/UAL/UAL1_Standard.plasset");
    auto grass = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Vegetation/GrassPatches/GrassPatches.plasset");
    auto vegetation = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Vegetation/GroundVegetation/GroundVegetation.plasset");
    auto jeep = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Props/Jeep/Jeep.plasset");

    if (!terrain || !joker || !ual || !grass || !vegetation || !jeep)
        throw std::runtime_error("OpenTerrain animation test required assets are not ready");

    PlagC::AnimationRetargetConfig retarget;
    retarget.rootTranslation = false;
    retarget.pelvisTranslation = false;
    retarget.translationScale = 1.0f;
    retarget.jointMap = JokerToUAL();
    playerAsset_ = PlagC::RetargetAnimations(joker, ual, retarget);

    // Fail immediately if any gameplay clips we depend on are missing.
    for (auto name : {"Idle_Loop", "Walk_Loop", "Jog_Fwd_Loop", "Sprint_Loop",
                      "Pistol_Aim_Neutral", "Pistol_Reload", "Pistol_Shoot"})
        (void)Clip(*playerAsset_, name);

    playerPosition_ = InitialPlayerPosition;
    playerYaw_ = 180.0f;
    locomotionSpeed_ = 0.0f;
    upperBodyWeight_ = 0.0f;
    reloadQueued_ = false;
    fireQueued_ = false;
    reloadTimer_ = 0.0;
    fireTimer_ = 0.0;

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    environment_ = ModelEntity(*world_, "Open Terrain", terrain,
                               {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}});

    player_ = ModelEntity(*world_, "Joker", playerAsset_,
                          {playerPosition_, {0, playerYaw_, 0},
                           {PlayerVisualScale, PlayerVisualScale, PlayerVisualScale}});

    PlagC::AnimatorComponent animator;
    auto graph = LocomotionGraph(*playerAsset_);
    animator.controller.emplace(graph, unsigned(playerAsset_->animations.size()));
    animator.state = {Clip(*playerAsset_, "Idle_Loop"), 0, 1, true, true};
    world_->SetAnimator(player_, animator);

    const PlagC::Vec3 vegetationSpots[] = {
        {-18.0f, 0.0f, -18.0f}, {-8.0f, 0.0f, -20.0f}, {8.0f, 0.0f, -20.0f},
        {18.0f, 0.0f, -18.0f}, {-20.0f, 0.0f, -7.0f}, {20.0f, 0.0f, -6.0f},
        {-20.0f, 0.0f, 8.0f}, {20.0f, 0.0f, 8.0f}, {-17.0f, 0.0f, 18.0f},
        {-7.0f, 0.0f, 20.0f}, {8.0f, 0.0f, 20.0f}, {18.0f, 0.0f, 17.0f}};

    for (unsigned i = 0; i < std::size(vegetationSpots); ++i)
    {
        auto p = vegetationSpots[i];
        p.y = TerrainHeight(p.x, p.z);
        const bool useGrass = (i % 2) == 0;
        auto asset = useGrass ? grass : vegetation;
        const float scale = useGrass ? 0.22f : 0.85f;
        ModelEntity(*world_, useGrass ? "Grass Patch" : "Ground Vegetation", asset,
                    {p, {0, float((i * 37) % 360), 0}, {scale, scale, scale}});
    }

    jeep_ = ModelEntity(*world_, "Jeep", jeep,
                        {{-15.0f, TerrainHeight(-15.0f, 10.0f) + 0.55f, 10.0f},
                         {0, 28, 0}, {1, 1, 1}});

    sun_ = world_->Create("Sun");
    world_->SetTransform(sun_, {{0, 12, 0}, {}, {1, 1, 1}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.42f, -0.86f, -0.28f};
    sun.colour = {1.0f, 0.95f, 0.86f};
    sun.power = 4.4f;
    world_->SetLight(sun_, {sun});

    camera_ = world_->Create("Gameplay Camera");
    world_->SetTransform(camera_, {{0.0f, 2.2f, 5.0f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {playerPosition_.x, playerPosition_.y + 1.0f, playerPosition_.z};
    camera.fov = 55.0f;
    camera.nearClip = 0.05f;
    camera.farClip = 350.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine](const PlagC::Event &event) {
        if (event.key == PlagC::KeyCode::R)
            reloadQueued_ = true;
        else if (event.key == PlagC::KeyCode::Escape)
        {
            cursorCaptured_ = !cursorCaptured_;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(cursorCaptured_);
        }
    });

    focus_ = engine.Events().Subscribe(PlagC::EventType::Focus,
                                       [this, &engine](const PlagC::Event &event) {
        if (!event.code)
        {
            cursorCaptured_ = false;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(false);
        }
    });

    mouse_ = engine.Events().Subscribe(PlagC::EventType::MousePressed,
                                       [this, &engine](const PlagC::Event &event) {
        if (event.button == PlagC::MouseCode::Left)
        {
            if (!cursorCaptured_)
            {
                cursorCaptured_ = true;
                if (auto *window = engine.MainWindow())
                    window->SetCursorCaptured(true);
            }
            else
                fireQueued_ = true;
        }
    });

    auto settings = engine.Graphics().Settings();
    settings.hdr = true;
    settings.bloom = true;
    settings.exposure = 1.0f;
    settings.fogDensity = 0.0010f;
    settings.fogColour = {0.32f, 0.37f, 0.40f};
    settings.background = {0.07f, 0.10f, 0.13f};
    engine.Graphics().SetSettings(settings);

    engine.SetFPSLocked(false);
    if (auto *window = engine.MainWindow())
    {
        window->SetTitle("PlagC | Survival Game | Blended Animation Test");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(
        PlagC::Severity::Info, "ANIMATION",
        "Joker gameplay animation ready: Ctrl+WASD walk, WASD jog, Shift+WASD sprint, RMB aim, LMB fire, R reload; lower-body crossfades + masked upper-body layer");
}

void CityStreetWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_ || !playerAsset_)
        throw std::logic_error("CityStreetWorld frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid CityStreetWorld timestep");
    dt = std::min(dt, 0.1);

    if (cursorCaptured_)
    {
        const auto mouse = PlagC::Input::GetMouseDelta();
        cameraYaw_ += float(mouse.x) * 0.12f;
        cameraPitch_ = std::clamp(cameraPitch_ - float(mouse.y) * 0.10f, -25.0f, 55.0f);
    }

    const float yaw = cameraYaw_ * Rad;
    const PlagC::Vec3 forward{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const PlagC::Vec3 right{std::cos(yaw), 0.0f, std::sin(yaw)};

    float f = 0.0f;
    float r = 0.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::W)) f += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::S)) f -= 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::D)) r += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::A)) r -= 1.0f;

    PlagC::Vec3 move{forward.x * f + right.x * r, 0.0f,
                     forward.z * f + right.z * r};
    float moveLength = LengthXZ(move);
    if (moveLength > 1.0f)
    {
        move.x /= moveLength;
        move.z /= moveLength;
        moveLength = 1.0f;
    }

    float desiredSpeed = 0.0f;
    if (moveLength > 0.001f)
    {
        const bool walk = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftControl) ||
                          PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightControl);
        const bool sprint = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                            PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift);
        desiredSpeed = sprint ? 6.2f : (walk ? 1.6f : 3.2f);

        playerPosition_.x = std::clamp(playerPosition_.x + move.x * desiredSpeed * float(dt),
                                       -TerrainHalfExtent, TerrainHalfExtent);
        playerPosition_.z = std::clamp(playerPosition_.z + move.z * desiredSpeed * float(dt),
                                       -TerrainHalfExtent, TerrainHalfExtent);

        const float desiredYaw = std::atan2(move.x, move.z) / Rad;
        playerYaw_ += std::remainder(desiredYaw - playerYaw_, 360.0f) *
                      (1.0f - std::exp(-float(dt) * 12.0f));
    }

    // Smooth the feature sent to the animation matcher so rapid key taps do not
    // thrash locomotion states, while world displacement remains responsive.
    const float speedResponse = desiredSpeed > locomotionSpeed_ ? 10.0f : 14.0f;
    locomotionSpeed_ += (desiredSpeed - locomotionSpeed_) *
                        (1.0f - std::exp(-float(dt) * speedResponse));
    if (std::abs(locomotionSpeed_) < .01f)
        locomotionSpeed_ = 0.0f;

    playerPosition_.y = TerrainHeight(playerPosition_.x, playerPosition_.z);
    world_->SetTransform(player_,
                         {playerPosition_, {0, playerYaw_, 0},
                          {PlayerVisualScale, PlayerVisualScale, PlayerVisualScale}});

    auto animator = world_->Read(player_).animator.value();
    if (!animator.controller)
        throw std::logic_error("Joker locomotion controller missing");
    animator.controller->SetFloat("Speed", locomotionSpeed_);

    const unsigned aimClip = Clip(*playerAsset_, "Pistol_Aim_Neutral");
    const unsigned reloadClip = Clip(*playerAsset_, "Pistol_Reload");
    const unsigned shootClip = Clip(*playerAsset_, "Pistol_Shoot");

    const double reloadDuration = playerAsset_->animations[reloadClip].duration;
    const double shootDuration = playerAsset_->animations[shootClip].duration;

    if (reloadQueued_ && reloadTimer_ <= 0.0)
    {
        reloadTimer_ = reloadDuration;
        fireTimer_ = 0.0;
    }
    reloadQueued_ = false;

    if (fireQueued_ && reloadTimer_ <= 0.0)
        fireTimer_ = shootDuration;
    fireQueued_ = false;

    if (reloadTimer_ > 0.0)
        reloadTimer_ = std::max(0.0, reloadTimer_ - dt);
    if (fireTimer_ > 0.0)
        fireTimer_ = std::max(0.0, fireTimer_ - dt);

    const bool aiming = PlagC::Input::IsMouseButtonPressed(PlagC::MouseCode::Right);
    const bool reloading = reloadTimer_ > 0.0;
    const bool firing = fireTimer_ > 0.0;
    const float targetUpperWeight = (aiming || reloading || firing) ? 1.0f : 0.0f;
    upperBodyWeight_ += (targetUpperWeight - upperBodyWeight_) *
                        (1.0f - std::exp(-float(dt) * 14.0f));
    if (upperBodyWeight_ < .001f)
        upperBodyWeight_ = 0.0f;

    unsigned upperClip = aimClip;
    double upperTime = 0.0;
    bool upperLoop = true;
    float recoil = 0.0f;

    if (reloading)
    {
        upperClip = reloadClip;
        upperTime = reloadDuration - reloadTimer_;
        upperLoop = false;
    }
    else if (firing)
    {
        upperClip = shootClip;
        upperTime = shootDuration - fireTimer_;
        upperLoop = false;
        recoil = 0.04f * float(std::sin(std::clamp(upperTime / std::max(.001, shootDuration), 0.0, 1.0) * 3.141592653589793));
    }

    PlagC::AnimationUpperBody upper;
    upper.pose = {upperClip, upperTime, 1.0f, true, upperLoop};
    upper.root = std::string(UpperBodyRoot);
    upper.weight = upperBodyWeight_;
    upper.recoilRadians = recoil;
    animator.upperBody = std::move(upper);
    world_->SetAnimator(player_, animator);

    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 lookDirection{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const PlagC::Vec3 target{playerPosition_.x, playerPosition_.y + 1.0f, playerPosition_.z};
    const float distance = 4.0f;
    const PlagC::Vec3 cameraPosition{target.x - lookDirection.x * distance,
                                     target.y - lookDirection.y * distance,
                                     target.z - lookDirection.z * distance};
    world_->SetTransform(camera_, {cameraPosition, {}, {1, 1, 1}});
    auto camera = world_->Read(camera_).camera.value();
    camera.target = target;
    world_->SetCamera(camera_, camera);

    assetScene_->Update(*world_, dt);
    sceneRenderer_->Sync(*world_);

    const auto animation = assetScene_->Statistics();
    engine.timings.animationMs = animation.animationMs;
    const auto sync = sceneRenderer_->Statistics();
    engine.timings.syncMs = sync.totalMs;
    engine.timings.syncStructureMs = sync.structureMs;
    engine.timings.syncResourceMs = sync.resourceMs;
    engine.timings.syncTransformMs = sync.transformMs;
    engine.timings.syncPropertyMs = sync.propertyMs;
    engine.timings.syncLightCameraMs = sync.lightCameraMs;
    engine.timings.syncTraversalMs = sync.traversalMs;
}

void CityStreetWorld::Stop(PlagC::Engine &engine)
{
    key_.Reset();
    focus_.Reset();
    mouse_.Reset();

    if (auto *window = engine.MainWindow())
        window->SetCursorCaptured(false);

    if (sceneRenderer_)
        sceneRenderer_->Clear();
    if (assetScene_)
        assetScene_->Clear();

    sceneRenderer_.reset();
    assetScene_.reset();
    physics_.reset();
    if (world_)
        world_->Clear();
    world_.reset();

    playerAsset_ = {};
    character_ = {};
    environment_ = {};
    player_ = {};
    jeep_ = {};
    camera_ = {};
    sun_ = {};

    engine.Logger().Write(PlagC::Severity::Info, "WORLD", "CityStreetWorld stopped");
}
} // namespace PlagCGame
