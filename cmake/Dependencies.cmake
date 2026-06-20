include(FetchContent)

find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)

FetchContent_Declare(
  ifcg
  GIT_REPOSITORY https://github.com/andrevbastos/ifcg.git
  GIT_TAG        main
)

FetchContent_MakeAvailable(ifcg)