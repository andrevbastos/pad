cmake_minimum_required(VERSION 3.20)
set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
project(ProjetoPAD VERSION 1.0.0 LANGUAGES C CXX)

# Configurações globais
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Organização dos binários (Padrão Nomai)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Caminho para módulos customizados
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Inclui o gerenciamento de dependências
include(Dependencies)

# Adiciona o código fonte do projeto
add_subdirectory(src)

# Habilita testes se necessário (desabilitado para o PAD)
# enable_testing()
# add_subdirectory(tests)