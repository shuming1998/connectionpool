
aux_source_directory(. SRC) # 存储当前目录下所有源文件

add_executable(ConnectionPoll ${SRC})

target_link_libraries(ConnectionPoll mysqlclient pthread)
