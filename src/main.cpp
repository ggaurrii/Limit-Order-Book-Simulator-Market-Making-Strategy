#include "lob/MatchingEngine.hpp"
#include <iostream>
#include <iomanip>

void print_book_state(const lob::OrderBook& book) {
    std::cout << "\n================ ORDER BOOK STATE ================\n";
    lob::Price bid_p = 0, ask_p = 0;
    lob::Quantity bid_v = 0, ask_v = 0;
    
    bool has_bid = book.get_best_bid(bid_p, bid_v);
    bool has_ask = book.get_best_ask(ask_p, ask_v);

    if (has_ask) {
        std::cout << "Best Ask: " << ask_p << " (Vol: " << ask_v << ")\n";
    } else {
        std::cout << "Best Ask: NONE\n";
    }

    if (has_bid) {
        std::cout << "Best Bid: " << bid_p << " (Vol: " << bid_v << ")\n";
    } else {
        std::cout << "Best Bid: NONE\n";
    }

    lob::Price spread = 0;
    if (book.get_spread(spread)) {
        std::cout << "Spread: " << spread << "\n";
    }

    std::cout << "\n--- ASKS (Top 5) ---\n";
    auto asks = book.get_ask_depth(5);
    for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
        std::cout << "Price: " << std::setw(8) << it->price 
                  << " | Vol: " << std::setw(6) << it->volume 
                  << " | Orders: " << it->order_count << "\n";
    }

    std::cout << "\n--- BIDS (Top 5) ---\n";
    auto bids = book.get_bid_depth(5);
    for (const auto& level : bids) {
        std::cout << "Price: " << std::setw(8) << level.price 
                  << " | Vol: " << std::setw(6) << level.volume 
                  << " | Orders: " << level.order_count << "\n";
    }
    std::cout << "Total Active Orders: " << book.total_order_count() << "\n";
    std::cout << "==================================================\n\n";
}

void print_events(const std::vector<lob::MarketEvent>& events) {
    for (const auto& evt : events) {
        std::cout << "[Seq #" << evt.sequence_number << "] ";
        switch (evt.event_type) {
            case lob::EventType::OrderAccepted:
                std::cout << "ACCEPTED OrderId=" << evt.order_id 
                          << " " << lob::to_string(evt.side)
                          << " P=" << evt.price << " Q=" << evt.quantity << "\n";
                break;
            case lob::EventType::OrderRejected:
                std::cout << "REJECTED OrderId=" << evt.order_id 
                          << " Reason: " << evt.reason << "\n";
                break;
            case lob::EventType::OrderExecuted:
                std::cout << "EXECUTION OrderId=" << evt.order_id 
                          << " TrdPrice=" << evt.price 
                          << " ExecQty=" << evt.quantity 
                          << " RemQty=" << evt.remaining_quantity 
                          << " Status=" << lob::to_string(evt.status) << "\n";
                break;
            case lob::EventType::TradeGenerated:
                std::cout << ">>> TRADE #" << evt.trade.trade_id 
                          << " BuyOrder=" << evt.trade.buy_order_id 
                          << " SellOrder=" << evt.trade.sell_order_id 
                          << " Price=" << evt.trade.price 
                          << " Qty=" << evt.trade.quantity << "\n";
                break;
            case lob::EventType::OrderCancelled:
                std::cout << "CANCELLED OrderId=" << evt.order_id 
                          << " RemQty=" << evt.quantity;
                if (!evt.reason.empty()) std::cout << " (" << evt.reason << ")";
                std::cout << "\n";
                break;
        }
    }
}

int main() {
    std::cout << "Limit Order Book & Matching Engine Demo (Phase 1)\n";

    lob::MatchingEngine engine;

    std::cout << "\n1. Submitting Limit Bids...\n";
    print_events(engine.submit_limit_order(101, lob::Side::Buy, 100, 50));
    print_events(engine.submit_limit_order(102, lob::Side::Buy, 101, 30));
    print_events(engine.submit_limit_order(103, lob::Side::Buy, 101, 20));

    std::cout << "\n2. Submitting Limit Asks...\n";
    print_events(engine.submit_limit_order(201, lob::Side::Sell, 105, 40));
    print_events(engine.submit_limit_order(202, lob::Side::Sell, 106, 60));

    print_book_state(engine.book());

    std::cout << "\n3. Submitting Crossing Limit Buy (Price 105, Qty 25)...\n";
    print_events(engine.submit_limit_order(104, lob::Side::Buy, 105, 25));

    print_book_state(engine.book());

    std::cout << "\n4. Submitting Market Sell (Qty 40)...\n";
    print_events(engine.submit_market_order(203, lob::Side::Sell, 40));

    print_book_state(engine.book());

    std::cout << "\n5. Cancelling Order #103...\n";
    print_events(engine.cancel_order(103));

    print_book_state(engine.book());

    return 0;
}
