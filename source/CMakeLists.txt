target_sources(ObfusC PRIVATE FuncAttribute.hpp)
target_sources(ObfusC PRIVATE FuncAttributeStore.hpp)
target_sources(ObfusC PRIVATE FuncAttributeStore.cpp)
target_sources(ObfusC PRIVATE common.hpp)
add_subdirectory("MBA")
add_subdirectory("BCF")
add_subdirectory("CFF")
add_subdirectory("ISUB")
add_subdirectory("IBR")
add_subdirectory("SPLIT")
add_subdirectory("ICALL")
add_subdirectory("ESTR")
add_subdirectory("UCD")
add_subdirectory("VIRT")
add_subdirectory("LSC")
add_subdirectory("Passes")