add_library (libs_filesystem STATIC
                                    directory.hpp
                                    )

set_target_properties (libs_filesystem PROPERTIES LINKER_LANGUAGE CXX)
