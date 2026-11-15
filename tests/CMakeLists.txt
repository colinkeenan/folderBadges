add_executable(classifiertest classifiertest.cpp)
target_link_libraries(classifiertest PRIVATE folderbadges-core Qt6::Test)
add_test(NAME classifiertest COMMAND classifiertest)
