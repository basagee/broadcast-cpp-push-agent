project(push_agent)

add_executable(push_agent 
        main.cpp 
        utils.cpp 
        SHA1.cpp 
        messagequeue.cpp 
        pushgateway.cpp 
        pushconnection.cpp 
        httplib.c)
target_link_libraries(push_agent -lrt -lpthread)

install(TARGETS push_agent RUNTIME DESTINATION bin)


add_subdirectory(rapidjson)