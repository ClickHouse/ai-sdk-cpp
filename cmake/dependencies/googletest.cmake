# Fetch Google Test using CPM (only if BUILD_TESTS is ON)
if(BUILD_TESTS)
    CPMAddPackage(
        NAME googletest
        GITHUB_REPOSITORY google/googletest
        VERSION 1.14.0
        OPTIONS
            "INSTALL_GTEST OFF"
            "gtest_force_shared_crt ON"
    )
    
    enable_testing()
endif()