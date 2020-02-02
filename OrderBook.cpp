#include "OrderBook.hpp"
#include <iostream>
#include <vector>
#include <sstream>


int main()
{
    std::string tick_size;
    std::getline(std::cin, tick_size);
    OrderBook book(tick_size);
    std::string line, token;
    
    while (std::getline(std::cin, line))
    {
	if (line.empty())
	    break;

	std::stringstream ss(line);
	std::vector<std::string> tokens; 

	while (ss >> token)
	{
	    tokens.push_back(token);
	}

	if (tokens[0] == "order")
	{
	    OrderId id = std::stoi(tokens[1]);
	    Side side = tokens[2] == "buy" ? Side::BUY : Side::SELL;
	    Qty qty = std::stoi(tokens[3]);
	    std::string price = tokens[4];
	    book.add_order(id, price, qty, side);
	}
	else if (tokens[0] == "cancel")
	{
	    OrderId id = std::stoi(tokens[1]);
	    book.cancel_order(id);
	}
	else if (tokens[0] == "amend")
	{
	    OrderId id = std::stoi(tokens[1]);
	    Qty qty = std::stoi(tokens[2]);
	    book.amend_order(id, qty);
	}
	else if (tokens[0] == "q" && tokens[1] == "level")
	{
	    Side side = tokens[2] == "bid" ? Side::BUY : Side::SELL;
	    uint32_t level = std::stoi(tokens[3]);
	    OrderBook::QueryLevelResult result;
	    if (book.query_level(side, level, result))
	    {
		std::cout << tokens[2] << ", "
			  << tokens[3] << ", "
			  << result.m_price << ", "
			  << result.m_total_qty << ", "
			  << result.m_num_orders << std::endl;
	    }
	}
	else if (tokens[0] == "q" && tokens[1] == "order")
	{
	    OrderId id = std::stoi(tokens[2]);
	    OrderBook::QueryOrderResult result;
	    if (book.query_order(id, result))
	    {
		switch (result.m_state)
		{
		case OrderBook::State::OPEN:
		    std::cout << "OPEN, ";
		    break;
		case OrderBook::State::PARTIALLY_FILLED:
		    std::cout << "PARTIALLY_FILLED, ";
		    break;
		case OrderBook::State::FULLY_FILLED:
		    std::cout << "FULLY_FILLED, ";
		    break;
		case OrderBook::State::CANCELLED:
		    std::cout << "CANCELLED, ";
		    break;
		}
		std::cout << result.m_leaves_qty << ", "
			  << result.m_queue_pos << std::endl;
	    }
	}
    }

    return 0;
}
