#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE const_string order_book_test
#include <boost/test/unit_test.hpp>

#include "OrderBook.hpp"

BOOST_AUTO_TEST_CASE(price_conversion)
{
    BOOST_TEST(OrderBook::to_price("123") == 123000000);
    BOOST_TEST(OrderBook::to_price("123.10") == 123100000);
    BOOST_TEST(OrderBook::to_price("123.1") == 123100000);
    BOOST_TEST(OrderBook::to_price("123.445") == 123445000);
    BOOST_TEST(OrderBook::to_price("123.3455678") == 123345567);
}

BOOST_AUTO_TEST_CASE(invalid_price)
{
    OrderBook book("0.5");
    OrderBook::QueryLevelResult result;

    book.add_order(1, "1.5", 10, Side::BUY);
    book.query_level(Side::BUY, 0, result);
    BOOST_TEST(result.m_num_orders == 1);

    book.add_order(2, "1.7", 10, Side::BUY); // Invalid tick size order
    book.query_level(Side::BUY, 0, result);
    BOOST_TEST(result.m_num_orders == 1);
}

BOOST_AUTO_TEST_CASE(matching)
{
    OrderBook book("0.5");
    OrderBook::QueryLevelResult result;

    book.add_order(1, "1.5", 10, Side::BUY);
    book.query_level(Side::BUY, 0, result);
    BOOST_TEST(result.m_num_orders == 1);

    book.add_order(2, "1.5", 3, Side::SELL);
    book.query_level(Side::BUY, 0, result);
    BOOST_TEST(result.m_num_orders == 1);
    BOOST_TEST(result.m_total_qty == 7);
}
