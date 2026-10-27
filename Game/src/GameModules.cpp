// Transitional compilation unit for compiled game modules.
// The current prototype Premake target already compiles Game/src/**.cpp.
// Keeping this bridge avoids changing the build graph during the first runtime-host refactor.
#include "../Framework/GameRegistry.cpp"
#include "../Default/DefaultGame.cpp"
#include "../SurvivalGame/SurvivalGame.cpp"
