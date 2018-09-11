#include "exchange.hpp"
#include "exchange_state.cpp"
#include "whitelisted.cpp"

#include <eosiolib/dispatcher.hpp>

namespace eosio {

    extended_asset min_asset(extended_asset a, extended_asset b) {
        return a < b ? a : b;
    }

    void exchange::on(const spec_trade &t) {
        auto base_symbol = t.receive.get_extended_symbol();
        auto quote_symbol = t.sell.get_extended_symbol();

        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.sell.is_valid(), "invalid sell amount");
        eosio_assert(t.receive.is_valid(), "invalid receive amount");
        eosio_assert(base_symbol != quote_symbol, "invalid exchange");

        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == base_symbol && itr->quote_symbol == quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");

        markets_table markets(_self, existing_pair->id);

        auto existing = markets.find(t.id);
        eosio_assert(existing != markets.end(), "Order with the specified primary key doesn't exist");
        eosio_assert(existing->base == t.receive, "Base deposits must be the same");
        eosio_assert(existing->quote == t.sell, "Base deposits must be the same");

        markets.erase(existing);

        _allowclaim(t.seller, t.sell);
        _claim(t.seller, existing->manager, t.sell);
        _claim(existing->manager, t.seller, t.receive);
    }

    void exchange::on(const market_trade &t) {
        auto base_symbol = t.receive.get_extended_symbol();
        auto quote_symbol = t.sell_symbol;

        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.receive.is_valid(), "invalid receive amount");
        eosio_assert(base_symbol != quote_symbol, "invalid exchange");

        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == base_symbol && itr->quote_symbol == quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");

        markets_table markets(_self, existing_pair->id);

        // market order: get X receive (base) for any sell (quote)
        eosio_assert(t.receive.amount > 0, "receive amount must be positive");
        extended_asset sold = extended_asset(0, quote_symbol);
        extended_asset received = extended_asset(0, base_symbol);
        extended_asset estimated_to_receive;

        auto sorted_markets = markets.get_index<N(byprice)>();
        auto order_itr = sorted_markets.cbegin();
        while (order_itr != sorted_markets.cend()) {
            auto order = *order_itr;
            if (order.manager == t.seller
                || order.quote.get_extended_symbol() != quote_symbol
                || order.base.get_extended_symbol() != base_symbol) {
                order_itr++;
                continue;
            }
            estimated_to_receive = t.receive - received;
            auto min = min_asset(order.base, estimated_to_receive);
            received += min;
            extended_asset output = order.convert(min, order.quote.get_extended_symbol());
            sold += output;

            if (min == order.base) {
                order_itr = sorted_markets.erase(order_itr);
            } else if (min < order.base) {
                sorted_markets.modify(order_itr, _self, [&](auto &s) {
                    s.base -= min;
                    s.quote -= output;
                });
            } else {
                eosio_assert(false, "incorrect state");
            }

            _allowclaim(t.seller, output);
            _claim(order.manager, t.seller, min);
            _claim(t.seller, order.manager, output);

            if (received == t.receive) break;
        }

        eosio_assert(received == t.receive, "unable to fill");
    }

    void exchange::on(const limit_trade &t) {
        auto base_symbol = t.receive_symbol;
        auto quote_symbol = t.sell.get_extended_symbol();

        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.sell.is_valid(), "invalid sell amount");
        eosio_assert(base_symbol != quote_symbol, "invalid exchange");

        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == base_symbol && itr->quote_symbol == quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");

        markets_table markets(_self, existing_pair->id);

        // limit order: get maximum receive (base) for X sell (quote)
        eosio_assert(t.sell.amount > 0, "sell amount must be positive");
        extended_asset sold = extended_asset(0, quote_symbol);
        extended_asset received = extended_asset(0, base_symbol);
        extended_asset estimated_to_sold;

        auto sorted_markets = markets.get_index<N(byprice)>();
        auto order_itr = sorted_markets.cbegin();
        while (order_itr != sorted_markets.cend()) {
            auto order = *order_itr;
            if (order.manager == t.seller
                || order.quote.get_extended_symbol() != quote_symbol
                || order.base.get_extended_symbol() != base_symbol) {
                order_itr++;
                continue;
            }
            estimated_to_sold = t.sell - sold;
            auto min = min_asset(order.quote, estimated_to_sold);
            sold += min;
            extended_asset output = order.convert(min, order.base.get_extended_symbol());
            received += output;

            if (min == order.quote) {
                order_itr = sorted_markets.erase(order_itr);
            } else if (min < order.quote) {
                sorted_markets.modify(order_itr, _self, [&](auto &s) {
                    s.base -= output;
                    s.quote -= min;
                });
            } else {
                eosio_assert(false, "incorrect state");
            }

            _allowclaim(t.seller, min);
            _claim(t.seller, order.manager, min);
            _claim(order.manager, t.seller, output);

            if (sold == t.sell) break;
        }

        eosio_assert(sold == t.sell, "unable to fill");
    }

    void exchange::on(const createx &c) {
        auto base_symbol = c.base_deposit.get_extended_symbol();
        auto quote_symbol = c.quote_deposit.get_extended_symbol();

        require_auth(c.creator);
        eosio_assert(is_whitelisted(c.creator), "Account is not whitelisted");
        eosio_assert(c.base_deposit.is_valid(), "invalid base deposit");
        eosio_assert(c.base_deposit.amount > 0, "base deposit must be positive");
        eosio_assert(c.quote_deposit.is_valid(), "invalid quote deposit");
        eosio_assert(c.quote_deposit.amount > 0, "quote deposit must be positive");
        eosio_assert(base_symbol != quote_symbol, "must exchange between two different currencies");

        // add pair if doesn't exist
        pairs_table pairs(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == base_symbol && itr->quote_symbol == quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        if (existing_pair == pairs.end()) {
            auto id = pairs.available_primary_key();
            pairs.emplace(_self, [&](auto& p) {
                p.id = id;
                p.base_symbol = base_symbol;
                p.quote_symbol = quote_symbol;
            });
            existing_pair = pairs.find(id);
        }

        _allowclaim(c.creator, c.base_deposit);

        print("base: ", c.base_deposit.get_extended_symbol(), '\n');
        print("quote: ", c.quote_deposit.get_extended_symbol(), '\n');

        auto price = (double) c.base_deposit.amount / c.quote_deposit.amount;

        auto markets = markets_table(_self, existing_pair->id);
        auto existing = markets.end();
        for (auto itr = markets.begin(); itr != markets.end(); itr++) {
            if (itr->manager == c.creator && itr->price == price) {
                existing = itr;
                break;
            }
        }

        if (existing == markets.end()) {
            print("create new trade\n");
            markets.emplace(c.creator, [&](auto &s) {
                s.id = markets.available_primary_key();
                s.manager = c.creator;
                s.base = c.base_deposit;
                s.quote = c.quote_deposit;
                s.price = price;
            });
        } else {
            print("combine trades with same rate\n");
            markets.modify(existing, _self, [&](auto &s) {
                s.base += c.base_deposit;
                s.quote += c.quote_deposit;
            });
        }
    }

    void exchange::on(const cancelx &c) {
        pairs_table pairs(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == c.base_symbol && itr->quote_symbol == c.quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        markets_table markets(_self, existing_pair->id);
        auto market = markets.find(c.id);
        eosio_assert(market != markets.end(), "order doesn't exist");
        require_auth(market->manager);
        markets.erase(market);

    }

    void exchange::_allowclaim(account_name owner, extended_asset quantity) {
        struct allowclaim {
            account_name from;
            account_name to;
            asset quantity;
        };

        action(eosio::vector<permission_level>({
                   permission_level(_self, N(active)),
                   permission_level(owner, N(active))
               }),
               quantity.contract,
               N(allowclaim),
               allowclaim{owner, _self, quantity}).send();
    }

    void exchange::_claim(account_name owner,
                          account_name to,
                          extended_asset quantity) {
        struct claim {
            account_name from;
            account_name to;
            asset quantity;
        };

        action(eosio::vector<permission_level>({
                   permission_level(_self, N(active)),
                   permission_level(to, N(active))
               }),
               quantity.contract,
               N(claim),
               claim{owner, to, quantity}).send();
    };

    void exchange::apply(account_name contract, account_name act) {
        if (contract != _self)
            return;

        auto &thiscontract = *this;
        switch (act) {
            EOSIO_API(exchange, (white)(unwhite)(whitemany)(unwhitemany))
        };

        switch (act) {
            case N(createx):
                on(unpack_action_data<createx>());
                return;
            case N(spec.trade):
                on(unpack_action_data<spec_trade>());
                return;
            case N(market.trade):
                on(unpack_action_data<market_trade>());
                return;
            case N(limit.trade):
                on(unpack_action_data<limit_trade>());
                return;
            case N(cancelx):
                on(unpack_action_data<cancelx>());
                return;
        }
    }
} /// namespace eosio

extern "C" {
    [[noreturn]] void apply(uint64_t receiver,
                            uint64_t code,
                            uint64_t action) {
        eosio::exchange ex(receiver);
        ex.apply(code, action);
        eosio_exit(0);
    }
}
