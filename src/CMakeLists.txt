# Coleta os fontes
file(GLOB PAD_SOURCES "*.cpp")

# Configurações de supressão de avisos para bibliotecas externas
if(TARGET ifcg_core)
    target_compile_options(ifcg_core PRIVATE -Wno-error)
endif()

# Itera sobre cada arquivo e cria um executável dinamicamente
foreach(SOURCE_FILE ${PAD_SOURCES})
    # Obtém o nome do arquivo sem a extensão
    get_filename_component(EXE_NAME ${SOURCE_FILE} NAME_WE)

    # Define o nome do alvo como o nome do executável
    set(TARGET_NAME ${EXE_NAME})

    # Cria o executável para o arquivo atual
    add_executable(${TARGET_NAME} ${SOURCE_FILE})

    # Flags de compilação rigorosas e otimização
    target_compile_options(${TARGET_NAME} PRIVATE -O3 -Wall -Wextra -Wpedantic -Werror)

    # Linka com os cores das bibliotecas importadas via FetchContent
    target_link_libraries(${TARGET_NAME} PRIVATE 
        ifcg_core 
    )

    # Inclui os headers do projeto
    target_include_directories(${TARGET_NAME} SYSTEM PRIVATE "${PROJECT_SOURCE_DIR}/include")

    # Copia recursos da engine ifcg para o diretório de build de cada executável (Essencial para Shaders)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${ifcg_SOURCE_DIR}/resources $<TARGET_FILE_DIR:${TARGET_NAME}>/resources
    )
endforeach()
