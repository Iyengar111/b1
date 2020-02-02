cmake_minimum_required(VERSION 3.10)

project (order_book)
find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})

add_executable(order_book OrderBook.cpp)
add_executable(order_book_test OrderBookTest.cpp)
target_link_libraries(order_book_test ${Boost_LIBRARIES})
