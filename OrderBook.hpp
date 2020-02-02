#pragma once

#include <unordered_map>
#include <map>
#include <list>
#include <cstdint>
#include <iterator>
#include <iostream>

using OrderId = uint64_t;
using Qty = uint64_t;
using Price = uint64_t;
enum class Side : uint8_t { BUY, SELL };


class OrderBook
{
public:
    enum class State : uint8_t { OPEN, PARTIALLY_FILLED, FULLY_FILLED, CANCELLED };

    struct QueryLevelResult
    {
	std::string m_price;
	Qty m_total_qty;
	size_t m_num_orders;
    };

    struct QueryOrderResult
    {
	State m_state;
	Qty m_leaves_qty;
	size_t m_queue_pos;
    };

    OrderBook(const std::string& tick_size) : m_tick_size(to_price(tick_size))
    {
    }
    
    void add_order(OrderId orderid, const std::string& price, Qty qty, Side side)
    {
	Price px = to_price(price);
	if (px % m_tick_size != 0)
	    return; // Invalid tick size.
	Qty original_qty = qty;
	OrderIt it;
	if (side == Side::BUY)
	{
	    auto f = [](Price aggresive, Price passive) { return aggresive >= passive; };
	    qty = match(m_sell_orders, f, px, qty);
	    if (qty > 0)
	    {
		PriceLevel& p = m_buy_orders[px];
		p.m_orders.emplace_back(Order{orderid, qty});
		p.m_total_qty += qty;
		it = std::prev(p.m_orders.end());
	    }
		
	}
	else
	{
	    auto f = [](Price aggresive, Price passive) { return aggresive <= passive; };
	    qty = match(m_buy_orders, f, px, qty);
	    if (qty > 0)
	    {
		PriceLevel& p = m_sell_orders[px];
		p.m_orders.emplace_back(Order{orderid, qty});
		p.m_total_qty += qty;
		it = std::prev(p.m_orders.end());
	    }
	}
	OrderDetail& od = m_order_detail[orderid];
	if (qty == original_qty)
	{
	    od.m_state = State::OPEN;
	}
	else if (qty > 0)
	{
	    od.m_state = State::PARTIALLY_FILLED;
	}
	else
	{
	    od.m_state = State::FULLY_FILLED;
	}
	od.m_price = px;
	od.m_side = side;
	od.m_it = it;
    }

    template <typename PriceLevels, typename CanHitFunctor>
    Qty match(PriceLevels& levels, CanHitFunctor can_hit, Price px, Qty qty)
    {
	auto level = levels.begin();
	while (level != levels.end())
	{
	    if (!can_hit(px, level->first))
	    {
		return qty;
	    }
	    auto& orders = level->second.m_orders;
	    auto order = orders.begin();
	    while (order != orders.end())
	    {
		Qty match_qty = std::min(qty, order->m_qty);
		order->m_qty -= match_qty;
		qty -= match_qty;
		level->second.m_total_qty -= match_qty;
		if (order->m_qty == 0)
		{
		    m_order_detail[order->m_orderid].m_state = State::FULLY_FILLED;
		    order = orders.erase(order);
		}
		else
		{
		    m_order_detail[order->m_orderid].m_state = State::PARTIALLY_FILLED;
		}
		if (qty == 0)
		{
		    break;
		}
	    }

	    if (level->second.m_total_qty == 0)
	    {
		level = levels.erase(level);
	    }
	    if (qty == 0)
	    {
		break;
	    }
	}

	return qty;
    }

    void cancel_order(OrderId orderid)
    {
	auto it = m_order_detail.find(orderid);
	if (it == m_order_detail.end())
	    return;
	OrderDetail& od = it->second;
	if (od.m_state == State::FULLY_FILLED || od.m_state == State::CANCELLED)
	    return;
	auto& price_level = od.m_side == Side::BUY ? m_buy_orders[od.m_price] : m_sell_orders[od.m_price];
	price_level.m_total_qty -= od.m_it->m_qty;
	price_level.m_orders.erase(od.m_it);
	od.m_state = State::CANCELLED;
	if (price_level.m_orders.empty())
	    m_buy_orders.erase(od.m_price);
    }
    
    void amend_order(OrderId orderid, Qty new_qty)
    {
	if (new_qty == 0)
	{
	    cancel_order(orderid);
	    return;
	}
	auto it = m_order_detail.find(orderid);
	if (it == m_order_detail.end())
	    return;
	OrderDetail& od = it->second;
	if (od.m_state == State::FULLY_FILLED || od.m_state == State::CANCELLED)
	    return;
	auto& price_level = od.m_side == Side::BUY ? m_buy_orders[od.m_price] : m_sell_orders[od.m_price];
	if (new_qty < od.m_it->m_qty)
	{
	    price_level.m_total_qty -= od.m_it->m_qty - new_qty;
	    od.m_it->m_qty = new_qty;
	}
	else
	{
	    price_level.m_total_qty += new_qty - od.m_it->m_qty;
	    price_level.m_orders.erase(od.m_it);
	    price_level.m_orders.push_back(Order{orderid, new_qty});
	    od.m_it = std::prev(price_level.m_orders.end());
	}
    }

    bool query_order(OrderId orderid, QueryOrderResult& result)
    {
	auto it = m_order_detail.find(orderid);
	if (it == m_order_detail.end())
	    return false;
	const OrderDetail& od = it->second;
	result.m_queue_pos = 0;
	result.m_state = od.m_state;
	if (od.m_state == State::FULLY_FILLED || od.m_state == State::CANCELLED)
	{
	    result.m_leaves_qty = 0;
	    return true;
	}
	const auto& orders = od.m_side == Side::BUY ? m_buy_orders[od.m_price].m_orders : m_sell_orders[od.m_price].m_orders;
	for (const auto& order : orders)
	{
	    if (order.m_orderid == orderid)
	    {
		result.m_leaves_qty = order.m_qty;
		return true;
	    }

	    result.m_queue_pos++;
	}

	return false;
    }

    
    bool query_level(Side side, uint32_t level, QueryLevelResult& result) const
    {
	auto begin = side == Side::BUY ? m_buy_orders.begin() : m_sell_orders.begin();
	auto end = side == Side::BUY ? m_buy_orders.end() : m_sell_orders.end();
	auto it = begin;
	for (uint32_t i = 0; i < level; i++)
	{
	    if (it == end)
	    {
		return false;
	    }
	    ++it;
	}
	if (it == end)
	{
	    return false;
	}

	result.m_price = to_string(it->first);
	result.m_total_qty = it->second.m_total_qty;
	result.m_num_orders = it->second.m_orders.size();

	return true;
    }

    static
    Price to_price(const std::string& px)
    {
	// Convert a string representation to a scaled integer (upto 6 decimal precision).
	size_t pos = px.find('.');
	if (pos == std::string::npos)
	{
	    return scaler * std::stoi(px);
	}
	else
	{
	    std::string whole = px.substr(0, pos);
	    std::string fraction = px.substr(pos + 1, std::min((size_t)6, px.length() - pos));
	    if (fraction.length() < 6)
		fraction.append(6 - fraction.length(), '0');
	    return (scaler * std::stoi(whole)) + std::stoi(fraction);
	}
    }

    static
    std::string to_string(Price px)
    {
	Price fraction = px % scaler;
	Price whole = px / scaler;
	return std::to_string(whole) + "." + std::to_string(fraction);
    }
    
private:
    struct BuyComparator
    {
	bool operator()(Price lhs, Price rhs) const
	{
	    return lhs > rhs;
	}
    };

    struct SellComparator
    {
	bool operator()(Price lhs, Price rhs) const
	{
	    return lhs < rhs;
	}
    };

    struct Order
    {
	OrderId m_orderid;
	Qty m_qty;
    };
    
    using Orders = std::list<Order>;
    using OrderIt = Orders::iterator;
    
    struct OrderDetail
    {
	State m_state;
	Price m_price;
	Side m_side;
	OrderIt m_it;
    };

    struct PriceLevel
    {
	Qty m_total_qty = 0;
	Orders m_orders;
    };

    static constexpr uint64_t scaler = 1000000; 

private:
    std::map<Price, PriceLevel, BuyComparator> m_buy_orders;
    std::map<Price, PriceLevel, SellComparator> m_sell_orders;
    std::unordered_map<OrderId, OrderDetail> m_order_detail;
    Price m_tick_size;
};
