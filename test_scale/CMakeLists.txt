cmake_minimum_required(VERSION 3.10)
project(scale_test C)

# Add include directory
include_directories(include)

# Add all source files
set(SOURCES
    src/main.c
    src/source_0.c
    src/source_1.c
    src/source_2.c
    src/source_3.c
    src/source_4.c
    src/source_5.c
    src/source_6.c
    src/source_7.c
    src/source_8.c
    src/source_9.c
    src/source_10.c
    src/source_11.c
    src/source_12.c
    src/source_13.c
    src/source_14.c
    src/source_15.c
    src/source_16.c
    src/source_17.c
    src/source_18.c
    src/source_19.c
    src/source_20.c
    src/source_21.c
    src/source_22.c
    src/source_23.c
    src/source_24.c
    src/source_25.c
    src/source_26.c
    src/source_27.c
    src/source_28.c
    src/source_29.c
    src/source_30.c
    src/source_31.c
    src/source_32.c
    src/source_33.c
    src/source_34.c
    src/source_35.c
    src/source_36.c
    src/source_37.c
    src/source_38.c
    src/source_39.c
    src/source_40.c
    src/source_41.c
    src/source_42.c
    src/source_43.c
    src/source_44.c
    src/source_45.c
    src/source_46.c
    src/source_47.c
    src/source_48.c
    src/source_49.c
    src/source_50.c
    src/source_51.c
    src/source_52.c
    src/source_53.c
    src/source_54.c
    src/source_55.c
    src/source_56.c
    src/source_57.c
    src/source_58.c
    src/source_59.c
    src/source_60.c
    src/source_61.c
    src/source_62.c
    src/source_63.c
    src/source_64.c
    src/source_65.c
    src/source_66.c
    src/source_67.c
    src/source_68.c
    src/source_69.c
    src/source_70.c
    src/source_71.c
    src/source_72.c
    src/source_73.c
    src/source_74.c
    src/source_75.c
    src/source_76.c
    src/source_77.c
    src/source_78.c
    src/source_79.c
    src/source_80.c
    src/source_81.c
    src/source_82.c
    src/source_83.c
    src/source_84.c
    src/source_85.c
    src/source_86.c
    src/source_87.c
    src/source_88.c
    src/source_89.c
    src/source_90.c
    src/source_91.c
    src/source_92.c
    src/source_93.c
    src/source_94.c
    src/source_95.c
    src/source_96.c
    src/source_97.c
    src/source_98.c
    src/source_99.c
)

# Create executable
add_executable(scale_test ${SOURCES})

# Set C standard
set_property(TARGET scale_test PROPERTY C_STANDARD 99)
